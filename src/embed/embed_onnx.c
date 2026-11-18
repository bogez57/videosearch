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
// are always on the processor: a query is some tens of milliseconds there,
// and the card would add its own start-up to each new shape of one.
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

   // The words that could be about anything, and the average of their
   // vectors (EMB_QueryVector), made in EMB_Load.
   String8* any_words;
   U64      any_words_count;
   F32      any_words_mean[EMB_DIM];

   // CLAP. No folder: it was not opened, and sound is not searched.
   String8        sound_folder;
   EMB_Tokenizer* sound_tokenizer;
   _EMB_Half      sound;      // input input_features; output audio_embeds
   _EMB_Half      sound_text; // as text
   U64            sound_loaded;

   // The words about sound, and what CLAP's text half makes of them and of
   // any_words (EMB_SoundQueryVector), made in EMB_LoadSound: the average
   // vector of each put as a query is, and for telling how much a query is
   // about sound, the vector of each as it is, less the average of them all
   // (sound_middle) and made length one.
   String8* sound_words;
   U64      sound_words_count;
   F32      sound_words_mean[EMB_DIM];
   F32      sound_middle[EMB_DIM];
   F32*     sound_words_from_middle; // sound_words_count by EMB_DIM
   F32*     any_words_from_middle;   // any_words_count by EMB_DIM
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

// A list of words' lines, copied: one a line, and a line that starts with #
// is not one of them.
internal String8*
_EMB_Lines(Arena* arena, String8 text, U64* count_out) {
   U64 lines = 1;
   for (U64 i = 0; i < text.size; i += 1) lines += (text.str[i] == '\n');
   String8* result = PushArray(arena, String8, lines);
   U64      count = 0;
   U64      pos = 0;
   while (pos < text.size) {
      U64 start = pos;
      while (pos < text.size && text.str[pos] != '\n') pos += 1;
      String8 line = Str8SkipChopWhitespace(Str8Substr(text, R1U64(start, pos)));
      pos += 1;
      if (line.size != 0 && line.str[0] != '#') {
         result[count++] = PushStr8Copy(arena, line);
      }
   }
   *count_out = count;
   return result;
}

internal EMB_Model*
EMB_Open(String8 folder, String8 merges_path, String8 any_words_path, B32 use_gpu) {
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

   model->any_words = _EMB_Lines(arena, DataFromFilePath(scratch.arena, any_words_path), &model->any_words_count);
   if (model->any_words_count == 0) {
      LogUserErrorF("No words that could be about anything at %S\n", any_words_path);
      ok = 0;
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

// count texts of size numbers each, one after another, through a text half:
// input_ids, and attention_mask (1 for a number that is part of the text, 0
// for one after it) if this export has one.
internal B32
_EMB_RunText(EMB_Model* model, _EMB_Half* half, int64_t* ids, int64_t* mask, U64 count, U64 size, F32* vectors_out) {
   B32       ok = 1;
   int64_t   shape[2] = {(int64_t)count, (int64_t)size};
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
                         model->ort->CreateTensorWithDataAsOrtValue(model->memory, data, count * size * sizeof(int64_t), shape,
                                                                    2, ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, &inputs[i]),
                         "encoding words");
   }
   ok = ok && _EMB_Run(model, half, inputs, count, vectors_out, "encoding words");
   for (U64 i = 0; i < half->input_count; i += 1) {
      if (inputs[i] != 0) model->ort->ReleaseValue(inputs[i]);
   }
   return ok;
}

// The picture model's text half's vectors of count texts (at most
// _EMB_TEXT_BATCH), each of length one. Every text goes in whole, its zeros
// too: this model's text half reads them all, as it did when it was trained.
#define _EMB_TEXT_BATCH 8
internal B32
_EMB_TextVectors(EMB_Model* model, String8* texts, U64 count, F32* vectors_out) {
   int64_t ids[_EMB_TEXT_BATCH * EMB_TEXT_TOKENS];
   int64_t mask[_EMB_TEXT_BATCH * EMB_TEXT_TOKENS];
   for (U64 t = 0; t < count; t += 1) {
      S32 ids32[EMB_TEXT_TOKENS];
      EMB_Tokenize(model->tokenizer, texts[t], ids32);
      B32 in_text = 1;
      for (U64 i = 0; i < EMB_TEXT_TOKENS; i += 1) {
         ids[t * EMB_TEXT_TOKENS + i] = ids32[i];
         mask[t * EMB_TEXT_TOKENS + i] = in_text;
         if (ids32[i] == EMB_TOKEN_END) in_text = 0;
      }
   }
   return _EMB_RunText(model, &model->text, ids, mask, count, EMB_TEXT_TOKENS, vectors_out);
}

internal B32
EMB_Load(EMB_Model* model) {
   B32 ok = _EMB_LoadHalf(model, &model->image, model->folder, "vision_model.onnx", "image_embeds", model->use_gpu);
   ok = ok && _EMB_LoadHalf(model, &model->text, model->folder, "text_model.onnx", "text_embeds", 0);

   // The average vector of the words that could be about anything.
   F64 sum[EMB_DIM] = {0};
   for (U64 at = 0; ok && at < model->any_words_count; at += _EMB_TEXT_BATCH) {
      F32 vectors[_EMB_TEXT_BATCH * EMB_DIM];
      U64 count = Min(_EMB_TEXT_BATCH, model->any_words_count - at);
      ok = _EMB_TextVectors(model, model->any_words + at, count, vectors);
      for (U64 i = 0; ok && i < count * EMB_DIM; i += 1) sum[i % EMB_DIM] += vectors[i];
   }
   for (U64 j = 0; ok && j < EMB_DIM; j += 1) {
      model->any_words_mean[j] = (F32)(sum[j] / (F64)model->any_words_count);
   }

   if (ok) {
      InsAtomicU64EvalAssign(&model->loaded, 1);
   }
   return ok;
}

// What a query's words are put after, for the text half to read them five
// ways (EMB_QueryVector). No more of them than _EMB_TEXT_BATCH.
read_only global String8 _emb_query_starts[] = {
    Str8LitComp(""),
    Str8LitComp("a photo of "),
    Str8LitComp("a video frame of "),
    Str8LitComp("a scene from a movie showing "),
    Str8LitComp("a picture of "),
};

internal B32
EMB_QueryVector(EMB_Model* model, String8 text, F32* vector_out) {
   B32 ok = EMB_IsLoaded(model);
   if (ok) {
      Temp    scratch = ScratchBegin(0, 0);
      String8 texts[ArrayCount(_emb_query_starts)];
      F32     vectors[ArrayCount(_emb_query_starts) * EMB_DIM];
      for (U64 i = 0; i < ArrayCount(texts); i += 1) {
         texts[i] = PushStr8F(scratch.arena, "%S%S", _emb_query_starts[i], text);
      }
      ok = _EMB_TextVectors(model, texts, ArrayCount(texts), vectors);

      // Their average made length one, less the average of the words that
      // could be about anything.
      F32 sum = 0;
      for (U64 j = 0; ok && j < EMB_DIM; j += 1) {
         F32 v = 0;
         for (U64 i = 0; i < ArrayCount(texts); i += 1) v += vectors[i * EMB_DIM + j];
         vector_out[j] = v;
         sum += v * v;
      }
      F32 inv = (sum > 0) ? 1.0f / sqrtf(sum) : 0;
      for (U64 j = 0; ok && j < EMB_DIM; j += 1) {
         vector_out[j] = vector_out[j] * inv - model->any_words_mean[j];
      }
      ScratchEnd(scratch);
   }
   return ok;
}

////////////////////////////////
//~ bgz: sound

internal B32
EMB_OpenSound(EMB_Model* model, String8 folder, String8 vocab_path, String8 merges_path, String8 sound_words_path) {
   B32  ok = (model != 0);
   Temp scratch = ScratchBegin(0, 0);
   if (ok && !FilePathExists(PushStr8F(scratch.arena, "%S\\audio_model.onnx", folder))) {
      LogInfoF("No sound model at %S, so sound is not searched\n", folder);
      ok = 0;
   }
   if (ok) {
      String8 vocab = DataFromFilePath(scratch.arena, vocab_path);
      String8 merges = DataFromFilePath(scratch.arena, merges_path);
      model->sound_words = _EMB_Lines(model->arena, DataFromFilePath(scratch.arena, sound_words_path), &model->sound_words_count);
      if (vocab.size == 0 || merges.size == 0) {
         LogUserErrorF("No sound tokenizer at %S and %S\n", vocab_path, merges_path);
         ok = 0;
      } else if (model->sound_words_count == 0) {
         LogUserErrorF("No words about sound at %S\n", sound_words_path);
         ok = 0;
      } else {
         // The arena is the main thread's, so what EMB_LoadSound fills is
         // made here.
         model->sound_words_from_middle = PushArray(model->arena, F32, model->sound_words_count * EMB_DIM);
         model->any_words_from_middle = PushArray(model->arena, F32, model->any_words_count * EMB_DIM);
         model->sound_tokenizer = EMB_SoundTokenizerFromVocab(model->arena, vocab, merges);
         model->sound_folder = PushStr8Copy(model->arena, folder);
      }
   }
   ScratchEnd(scratch);
   return ok;
}

// CLAP's text half's vector of text, of length one.
internal B32
_EMB_SoundTextVector(EMB_Model* model, String8 text, F32* vector_out) {
   S32     ids[EMB_TEXT_TOKENS];
   int64_t ids64[EMB_TEXT_TOKENS];
   int64_t mask[EMB_TEXT_TOKENS];
   U64     size = EMB_SoundTokenize(model->sound_tokenizer, text, ids);
   for (U64 i = 0; i < size; i += 1) {
      ids64[i] = ids[i];
      mask[i] = 1;
   }
   return _EMB_RunText(model, &model->sound_text, ids64, mask, 1, size, vector_out);
}

// Words put the way a query for sound is: the sentence CLAP's makers tested
// it with.
internal String8
_EMB_SoundSentence(Arena* arena, String8 text) {
   return PushStr8F(arena, "This is a sound of %S.", text);
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
      // Both lists of words as they are, each less the middle of them all.
      Temp scratch = ScratchBegin(0, 0);
      U64  count = model->sound_words_count + model->any_words_count;
      F64  middle[EMB_DIM] = {0};
      for (U64 i = 0; ok && i < count; i += 1) {
         B32  is_sound = (i < model->sound_words_count);
         U64  at = is_sound ? i : i - model->sound_words_count;
         F32* v = (is_sound ? model->sound_words_from_middle : model->any_words_from_middle) + at * EMB_DIM;
         ok = _EMB_SoundTextVector(model, is_sound ? model->sound_words[at] : model->any_words[at], v);
         for (U64 j = 0; ok && j < EMB_DIM; j += 1) middle[j] += v[j];
      }
      for (U64 j = 0; ok && j < EMB_DIM; j += 1) model->sound_middle[j] = (F32)(middle[j] / (F64)count);
      for (U64 i = 0; ok && i < count; i += 1) {
         B32  is_sound = (i < model->sound_words_count);
         F32* v = is_sound ? model->sound_words_from_middle + i * EMB_DIM
                           : model->any_words_from_middle + (i - model->sound_words_count) * EMB_DIM;
         F32  sum = 0;
         for (U64 j = 0; j < EMB_DIM; j += 1) {
            v[j] -= model->sound_middle[j];
            sum += v[j] * v[j];
         }
         F32 inv = (sum > 0) ? 1.0f / sqrtf(sum) : 0;
         for (U64 j = 0; j < EMB_DIM; j += 1) v[j] *= inv;
      }

      // The words about sound put as a query is: their average.
      F64 mean[EMB_DIM] = {0};
      for (U64 i = 0; ok && i < model->sound_words_count; i += 1) {
         F32 v[EMB_DIM];
         ok = _EMB_SoundTextVector(model, _EMB_SoundSentence(scratch.arena, model->sound_words[i]), v);
         for (U64 j = 0; ok && j < EMB_DIM; j += 1) mean[j] += v[j];
      }
      for (U64 j = 0; ok && j < EMB_DIM; j += 1) {
         model->sound_words_mean[j] = (F32)(mean[j] / (F64)model->sound_words_count);
      }
      ScratchEnd(scratch);
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

// How near v is to a list of words' vectors (count by EMB_DIM): the average
// of its dot products with the five nearest of them after the very nearest,
// which may be the same words.
internal F32
_EMB_NearnessToWords(F32* v, F32* words, U64 count) {
   F32 nearest[6] = {-2, -2, -2, -2, -2, -2}; // highest first
   for (U64 i = 0; i < count; i += 1) {
      F32* w = words + i * EMB_DIM;
      F32  dot = 0;
      for (U64 j = 0; j < EMB_DIM; j += 1) dot += v[j] * w[j];
      for (U64 k = 0; k < ArrayCount(nearest); k += 1) {
         if (dot > nearest[k]) {
            F32 lower = nearest[k];
            nearest[k] = dot;
            dot = lower;
         }
      }
   }
   F32 sum = 0;
   U64 used = 0;
   for (U64 k = 1; k < Min(count, ArrayCount(nearest)); k += 1) {
      sum += nearest[k];
      used += 1;
   }
   return used ? sum / (F32)used : 0;
}

internal B32
EMB_SoundQueryVector(EMB_Model* model, String8 text, F32* vector_out, F32* about_sound_out) {
   B32 ok = EMB_SoundIsLoaded(model);
   if (ok) {
      Temp scratch = ScratchBegin(0, 0);
      F32  as_it_is[EMB_DIM];
      ok = _EMB_SoundTextVector(model, _EMB_SoundSentence(scratch.arena, text), vector_out) &&
           _EMB_SoundTextVector(model, text, as_it_is);
      if (ok) {
         F32 sum = 0;
         for (U64 j = 0; j < EMB_DIM; j += 1) {
            vector_out[j] -= model->sound_words_mean[j];
            as_it_is[j] -= model->sound_middle[j];
            sum += as_it_is[j] * as_it_is[j];
         }
         F32 inv = (sum > 0) ? 1.0f / sqrtf(sum) : 0;
         for (U64 j = 0; j < EMB_DIM; j += 1) as_it_is[j] *= inv;
         *about_sound_out = _EMB_NearnessToWords(as_it_is, model->sound_words_from_middle, model->sound_words_count) -
                            _EMB_NearnessToWords(as_it_is, model->any_words_from_middle, model->any_words_count);
      }
      ScratchEnd(scratch);
   }
   return ok;
}
