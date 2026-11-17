////////////////////////////////
//~ bgz: embed: the model, run with ONNX Runtime
//
// The functions of embed.h that run the model. This is the only file in the
// program that includes ONNX Runtime's header or calls it.
//
// ONNX Runtime is src\third_party\onnxruntime (1.23.0, Microsoft's DirectML
// build for x64 Windows): its C headers, the import library the program
// links against, and onnxruntime.dll and DirectML.dll, which build.bat
// copies next to the exe. Its memory is its own, let go of in EMB_Close.
//
// The halves that indexing runs - pictures and sound - are run on the
// graphics card, through DirectML (any card that does Direct3D 12), and on
// the processor if that can't be had or was not asked for. The text halves
// are always on the processor: a query is a few milliseconds there, and the
// card would add its own start-up to each new length of one.
//
// Every call of ONNX Runtime's returns an OrtStatus: 0, or what went wrong.
// One half is run by one thread at a time, which DirectML needs.

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Weverything"
#endif
#include "onnxruntime_c_api.h"
#if defined(__clang__)
# pragma clang diagnostic pop
#endif

// One half of the model: its inputs' names (zero-ended) and the output used.
typedef struct _EMB_Half _EMB_Half;
struct _EMB_Half {
   OrtSession* session;
   String8     inputs[4];
   U64         input_count;
   String8     output;
};

struct EMB_Model {
   Arena*         arena;
   String8        folder;
   EMB_Tokenizer* tokenizer;
   const OrtApi*  ort;
   OrtEnv*        env;
   OrtMemoryInfo* memory; // says the tensors handed to it are in ordinary memory
   _EMB_Half      image;  // input pixel_values; output image_embeds
   _EMB_Half      text;   // inputs input_ids and maybe attention_mask; output text_embeds
   U64            loaded; // set once both halves are, read with an atomic
   B32            use_gpu;

   // CLAP. No folder: it was not opened, and sound is not searched.
   String8        sound_folder;
   EMB_Tokenizer* sound_tokenizer;
   _EMB_Half      sound;      // input input_features; output audio_embeds
   _EMB_Half      sound_text; // as text
   U64            sound_loaded;
};

// Logs what went wrong, if anything, and lets go of the status.
internal B32
_EMB_Ok(EMB_Model* model, OrtStatus* status, char* doing) {
   B32 result = (status == 0);
   if (!result) {
      LogUserErrorF("ONNX Runtime, %s: %s\n", doing, model->ort->GetErrorMessage(status));
      model->ort->ReleaseStatus(status);
   }
   return result;
}

internal void
EMB_Close(EMB_Model* model) {
   if (model != 0) {
      if (model->image.session != 0) model->ort->ReleaseSession(model->image.session);
      if (model->text.session != 0) model->ort->ReleaseSession(model->text.session);
      if (model->sound.session != 0) model->ort->ReleaseSession(model->sound.session);
      if (model->sound_text.session != 0) model->ort->ReleaseSession(model->sound_text.session);
      if (model->memory != 0) model->ort->ReleaseMemoryInfo(model->memory);
      if (model->env != 0) model->ort->ReleaseEnv(model->env);
      ArenaRelease(model->arena);
   }
}

internal EMB_Model*
EMB_Open(String8 folder, String8 merges_path, B32 use_gpu) {
   Temp       scratch = ScratchBegin(0, 0);
   Arena*     arena = ArenaAlloc();
   EMB_Model* model = PushArray(arena, EMB_Model, 1);
   model->arena = arena;
   model->folder = PushStr8Copy(arena, folder);
   model->use_gpu = use_gpu;
   B32 ok = 1;

   String8 merges = DataFromFilePath(scratch.arena, merges_path);
   if (merges.size == 0) {
      LogUserErrorF("No tokenizer merges at %S\n", merges_path);
      ok = 0;
   } else {
      model->tokenizer = EMB_TokenizerFromMerges(arena, merges);
   }

   // An onnxruntime.dll older than the header (Windows can have one in
   // System32) gives no API of this version.
   if (ok) {
      model->ort = OrtGetApiBase()->GetApi(ORT_API_VERSION);
      if (model->ort == 0) {
         LogUserErrorF("ONNX Runtime: onnxruntime.dll is older than %d - is the one in bin the one loaded?\n", ORT_API_VERSION);
         ok = 0;
      }
   }
   ok = ok && _EMB_Ok(model, model->ort->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "videosearch", &model->env), "starting");
   ok = ok && _EMB_Ok(model, model->ort->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &model->memory),
                      "starting");

   if (!ok) {
      if (model->ort != 0) {
         EMB_Close(model);
      } else {
         ArenaRelease(arena);
      }
      model = 0;
   }
   ScratchEnd(scratch);
   return model;
}

// A session for the model file at path. With gpu, one that runs on the
// graphics card: 0 if it can't, and why is logged as something to know.
internal B32
_EMB_CreateSession(EMB_Model* model, wchar_t* path, char* file, B32 gpu, OrtSession** session_out) {
   OrtSessionOptions* options = 0;
   B32                ok = _EMB_Ok(model, model->ort->CreateSessionOptions(&options), "starting");
   ok = ok && _EMB_Ok(model, model->ort->SetSessionGraphOptimizationLevel(options, ORT_ENABLE_ALL), "starting");
   if (ok && gpu) {
      // The card that is fastest, if there are two. DirectML wants the two
      // settings after it.
      const char* keys[] = {"performance_preference"};
      const char* values[] = {"high_performance"};
      OrtStatus*  status = model->ort->SessionOptionsAppendExecutionProvider(options, "DML", keys, values, 1);
      if (status == 0) status = model->ort->DisableMemPattern(options);
      if (status == 0) status = model->ort->SetSessionExecutionMode(options, ORT_SEQUENTIAL);
      if (status == 0) status = model->ort->CreateSession(model->env, path, options, session_out);
      if (status != 0) {
         LogInfoF("%s can't run on the graphics card, so it runs on the processor: %s\n", file,
                  model->ort->GetErrorMessage(status));
         model->ort->ReleaseStatus(status);
         ok = 0;
      }
   } else if (ok) {
      ok = _EMB_Ok(model, model->ort->CreateSession(model->env, path, options, session_out), file);
   }
   if (options != 0) model->ort->ReleaseSessionOptions(options);
   return ok;
}

// Loads file from folder into half, with the names of its inputs and of the
// output named output. With gpu it runs on the graphics card if it can.
internal B32
_EMB_LoadHalf(EMB_Model* model, _EMB_Half* half, String8 folder, char* file, char* output, B32 gpu) {
   Temp    scratch = ScratchBegin(0, 0);
   String8 path = PushStr8F(scratch.arena, "%S\\%s", folder, file);
   B32     ok = 1;
   if (!FilePathExists(path)) {
      LogUserErrorF("No model at %S\n", path);
      ok = 0;
   }

   if (ok) {
      // Its paths are UTF-16 on Windows.
      String32 path32 = Str32From8(scratch.arena, path);
      wchar_t* wide = PushArray(scratch.arena, wchar_t, path32.size * 2 + 1);
      U64      wide_size = 0;
      for (U64 i = 0; i < path32.size; i += 1) {
         U32 c = path32.str[i];
         if (c >= 0x10000) {
            c -= 0x10000;
            wide[wide_size++] = (wchar_t)(0xD800 + (c >> 10));
            wide[wide_size++] = (wchar_t)(0xDC00 + (c & 0x3FF));
         } else {
            wide[wide_size++] = (wchar_t)c;
         }
      }
      B32 on_gpu = (gpu && _EMB_CreateSession(model, wide, file, 1, &half->session));
      ok = on_gpu || _EMB_CreateSession(model, wide, file, 0, &half->session);
      if (on_gpu) {
         LogInfoF("%s runs on the graphics card\n", file);
      }
   }

   if (ok) {
      OrtAllocator* allocator = 0;
      ok = _EMB_Ok(model, model->ort->GetAllocatorWithDefaultOptions(&allocator), file);

      size_t input_count = 0;
      ok = ok && _EMB_Ok(model, model->ort->SessionGetInputCount(half->session, &input_count), file);
      if (ok && input_count > ArrayCount(half->inputs)) {
         LogUserErrorF("%s has %llu inputs\n", file, (unsigned long long)input_count);
         ok = 0;
      }
      for (size_t i = 0; ok && i < input_count; i += 1) {
         char* name = 0;
         ok = _EMB_Ok(model, model->ort->SessionGetInputName(half->session, i, allocator, &name), file);
         if (ok) {
            half->inputs[half->input_count++] = PushStr8Copy(model->arena, Str8CString(name));
            model->ort->AllocatorFree(allocator, name);
         }
      }

      size_t      output_count = 0;
      String8List names = {0};
      ok = ok && _EMB_Ok(model, model->ort->SessionGetOutputCount(half->session, &output_count), file);
      for (size_t i = 0; ok && i < output_count; i += 1) {
         char* name = 0;
         ok = _EMB_Ok(model, model->ort->SessionGetOutputName(half->session, i, allocator, &name), file);
         if (ok) {
            String8 s = PushStr8Copy(model->arena, Str8CString(name));
            model->ort->AllocatorFree(allocator, name);
            Str8ListPush(scratch.arena, &names, s);
            if (Str8Match(s, Str8CString(output), 0)) {
               half->output = s;
            }
         }
      }
      if (ok && half->output.size == 0) {
         StringJoin join = {0};
         join.sep = Str8Lit(", ");
         LogUserErrorF("%s has no output %s, only: %S\n", file, output, Str8ListJoin(scratch.arena, &names, &join));
         ok = 0;
      }
   }

   ScratchEnd(scratch);
   return ok;
}

internal B32
EMB_Load(EMB_Model* model) {
   B32 ok = _EMB_LoadHalf(model, &model->image, model->folder, "vision_model.onnx", "image_embeds", model->use_gpu);
   ok = ok && _EMB_LoadHalf(model, &model->text, model->folder, "text_model.onnx", "text_embeds", 0);
   if (ok) {
      InsAtomicU64EvalAssign(&model->loaded, 1);
   }
   return ok;
}

internal B32
EMB_IsLoaded(EMB_Model* model) {
   return model != 0 && InsAtomicU64Eval(&model->loaded) != 0;
}

// Runs half on its inputs and copies out its output, count vectors of
// EMB_DIM, each made length one.
internal B32
_EMB_Run(EMB_Model* model, _EMB_Half* half, OrtValue** inputs, U64 count, F32* vectors_out, char* doing) {
   OrtValue*   output = 0;
   const char* input_names[ArrayCount(half->inputs)];
   for (U64 i = 0; i < half->input_count; i += 1) {
      input_names[i] = (const char*)half->inputs[i].str;
   }
   const char* output_name = (const char*)half->output.str;
   B32         ok = _EMB_Ok(model,
                            model->ort->Run(half->session, 0, input_names, (const OrtValue* const*)inputs, half->input_count,
                                            &output_name, 1, &output),
                            doing);

   // The vectors are count by EMB_DIM. Checked, since the model is a file
   // that could be another one.
   F32* data = 0;
   if (ok) {
      OrtTensorTypeAndShapeInfo* info = 0;
      ok = _EMB_Ok(model, model->ort->GetTensorTypeAndShape(output, &info), doing);
      size_t  dims_count = 0;
      int64_t dims[2] = {0};
      ok = ok && _EMB_Ok(model, model->ort->GetDimensionsCount(info, &dims_count), doing);
      if (ok && dims_count == 2) {
         ok = _EMB_Ok(model, model->ort->GetDimensions(info, dims, 2), doing);
      }
      if (info != 0) model->ort->ReleaseTensorTypeAndShapeInfo(info);
      if (ok && (dims_count != 2 || dims[0] != (int64_t)count || dims[1] != EMB_DIM)) {
         LogUserErrorF("ONNX Runtime, %s: the vectors are not %llu by %d\n", doing, (unsigned long long)count, EMB_DIM);
         ok = 0;
      }
      ok = ok && _EMB_Ok(model, model->ort->GetTensorMutableData(output, (void**)&data), doing);
   }

   if (ok) {
      for (U64 i = 0; i < count; i += 1) {
         F32* v = data + i * EMB_DIM;
         F32  sum = 0;
         for (U64 j = 0; j < EMB_DIM; j += 1) sum += v[j] * v[j];
         F32 inv = (sum > 0) ? 1.0f / sqrtf(sum) : 0;
         for (U64 j = 0; j < EMB_DIM; j += 1) vectors_out[i * EMB_DIM + j] = v[j] * inv;
      }
   }

   if (output != 0) model->ort->ReleaseValue(output);
   return ok;
}

internal B32
EMB_ImageVectors(EMB_Model* model, F32* images, U64 count, F32* vectors_out) {
   OrtValue* input = 0;
   int64_t   shape[4] = {(int64_t)count, 3, EMB_IMAGE_DIM, EMB_IMAGE_DIM};
   B32       ok = EMB_IsLoaded(model);
   ok = ok && _EMB_Ok(model,
                      model->ort->CreateTensorWithDataAsOrtValue(model->memory, images, count * EMB_IMAGE_FLOATS * sizeof(F32),
                                                                 shape, 4, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &input),
                      "encoding pictures");
   ok = ok && _EMB_Run(model, &model->image, &input, count, vectors_out, "encoding pictures");
   if (input != 0) model->ort->ReleaseValue(input);
   return ok;
}

// size numbers of words through a text half: input_ids, and attention_mask
// if this export has one.
internal B32
_EMB_RunText(EMB_Model* model, _EMB_Half* half, int64_t* ids, U64 size, F32* vector_out) {
   B32     ok = 1;
   int64_t mask[EMB_TEXT_TOKENS];
   for (U64 i = 0; i < size; i += 1) mask[i] = 1;

   int64_t   shape[2] = {1, (int64_t)size};
   OrtValue* inputs[ArrayCount(half->inputs)] = {0};
   for (U64 i = 0; ok && i < half->input_count; i += 1) {
      String8 name = half->inputs[i];
      void*   data = 0;
      if (Str8Match(name, Str8Lit("input_ids"), 0)) {
         data = ids;
      } else if (Str8Match(name, Str8Lit("attention_mask"), 0)) {
         data = mask;
      } else {
         LogUserErrorF("The text model has an input %S, which is not known here\n", name);
         ok = 0;
      }
      ok = ok && _EMB_Ok(model,
                         model->ort->CreateTensorWithDataAsOrtValue(model->memory, data, size * sizeof(int64_t), shape, 2,
                                                                    ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, &inputs[i]),
                         "encoding words");
   }
   ok = ok && _EMB_Run(model, half, inputs, 1, vector_out, "encoding words");
   for (U64 i = 0; i < half->input_count; i += 1) {
      if (inputs[i] != 0) model->ort->ReleaseValue(inputs[i]);
   }
   return ok;
}

internal B32
EMB_TextVector(EMB_Model* model, String8 text, F32* vector_out) {
   B32 ok = EMB_IsLoaded(model);
   if (ok) {
      S32 ids[EMB_TEXT_TOKENS];
      EMB_Tokenize(model->tokenizer, text, ids);

      // Only up to the end token: the model reads the words one way, so what
      // comes after the end changes nothing, and a shorter input is quicker.
      int64_t ids64[EMB_TEXT_TOKENS];
      U64     size = 0;
      while (size < EMB_TEXT_TOKENS) {
         ids64[size] = ids[size];
         size += 1;
         if (ids[size - 1] == EMB_TOKEN_END) break;
      }
      ok = _EMB_RunText(model, &model->text, ids64, size, vector_out);
   }
   return ok;
}

////////////////////////////////
//~ bgz: sound

internal B32
EMB_OpenSound(EMB_Model* model, String8 folder, String8 vocab_path, String8 merges_path) {
   B32  ok = (model != 0);
   Temp scratch = ScratchBegin(0, 0);
   if (ok && !FilePathExists(PushStr8F(scratch.arena, "%S\\audio_model.onnx", folder))) {
      LogInfoF("No sound model at %S, so sound is not searched\n", folder);
      ok = 0;
   }
   if (ok) {
      String8 vocab = DataFromFilePath(scratch.arena, vocab_path);
      String8 merges = DataFromFilePath(scratch.arena, merges_path);
      if (vocab.size == 0 || merges.size == 0) {
         LogUserErrorF("No sound tokenizer at %S and %S\n", vocab_path, merges_path);
         ok = 0;
      } else {
         model->sound_tokenizer = EMB_SoundTokenizerFromVocab(model->arena, vocab, merges);
         model->sound_folder = PushStr8Copy(model->arena, folder);
      }
   }
   ScratchEnd(scratch);
   return ok;
}

internal B32
EMB_LoadSound(EMB_Model* model) {
   B32 ok = (model != 0 && model->sound_folder.size != 0);
   ok = ok && _EMB_LoadHalf(model, &model->sound, model->sound_folder, "audio_model.onnx", "audio_embeds", model->use_gpu);
   ok = ok && _EMB_LoadHalf(model, &model->sound_text, model->sound_folder, "text_model.onnx", "text_embeds", 0);
   if (ok && model->sound.input_count != 1) {
      LogUserErrorF("The sound model has %llu inputs, not the one known here\n", (unsigned long long)model->sound.input_count);
      ok = 0;
   }
   if (ok) {
      InsAtomicU64EvalAssign(&model->sound_loaded, 1);
   }
   return ok;
}

internal B32
EMB_SoundIsLoaded(EMB_Model* model) {
   return model != 0 && InsAtomicU64Eval(&model->sound_loaded) != 0;
}

internal B32
EMB_SoundVector(EMB_Model* model, F32* features, F32* vector_out) {
   OrtValue* input = 0;
   int64_t   shape[4] = {1, 1, EMB_SOUND_FRAMES, EMB_SOUND_MELS};
   B32       ok = EMB_SoundIsLoaded(model);
   ok = ok && _EMB_Ok(model,
                      model->ort->CreateTensorWithDataAsOrtValue(model->memory, features, EMB_SOUND_FLOATS * sizeof(F32), shape,
                                                                 4, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &input),
                      "encoding sound");
   ok = ok && _EMB_Run(model, &model->sound, &input, 1, vector_out, "encoding sound");
   if (input != 0) model->ort->ReleaseValue(input);
   return ok;
}

internal B32
EMB_SoundTextVector(EMB_Model* model, String8 text, F32* vector_out) {
   B32 ok = EMB_SoundIsLoaded(model);
   if (ok) {
      S32     ids[EMB_TEXT_TOKENS];
      int64_t ids64[EMB_TEXT_TOKENS];
      U64     size = EMB_SoundTokenize(model->sound_tokenizer, text, ids);
      for (U64 i = 0; i < size; i += 1) ids64[i] = ids[i];
      ok = _EMB_RunText(model, &model->sound_text, ids64, size, vector_out);
   }
   return ok;
}
