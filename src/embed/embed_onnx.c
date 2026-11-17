////////////////////////////////
//~ bgz: embed: the model, run with ONNX Runtime
//
// The functions of embed.h that run the model. This is the only file in the
// program that includes ONNX Runtime's header or calls it.
//
// ONNX Runtime is src\third_party\onnxruntime (1.23.2, Microsoft's build for
// x64 Windows): its C headers, the import library the program links against,
// and onnxruntime.dll, which build.bat copies next to the exe. It runs the
// model on the processor, on as many threads as it has cores. Its memory is
// its own, let go of in EMB_Close.
//
// Every call of ONNX Runtime's returns an OrtStatus: 0, or what went wrong.

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Weverything"
#endif
#include "onnxruntime_c_api.h"
#if defined(__clang__)
# pragma clang diagnostic pop
#endif

struct EMB_Model {
   const OrtApi*  ort;
   OrtEnv*        env;
   OrtMemoryInfo* memory; // says the tensors handed to it are in ordinary memory
   OrtSession*    image;
   String8        image_input;  // the names of the image model's input and the output used
   String8        image_output;
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
      if (model->image != 0) model->ort->ReleaseSession(model->image);
      if (model->memory != 0) model->ort->ReleaseMemoryInfo(model->memory);
      if (model->env != 0) model->ort->ReleaseEnv(model->env);
   }
}

internal EMB_Model*
EMB_Open(Arena* arena, String8 folder) {
   Temp       scratch = ScratchBegin(&arena, 1);
   EMB_Model* model = PushArray(arena, EMB_Model, 1);
   B32        ok = 1;

   // An onnxruntime.dll older than the header (Windows can have one in
   // System32) gives no API of this version.
   model->ort = OrtGetApiBase()->GetApi(ORT_API_VERSION);
   if (model->ort == 0) {
      LogUserErrorF("ONNX Runtime: onnxruntime.dll is older than %d - is the one in bin the one loaded?\n", ORT_API_VERSION);
      ok = 0;
   }

   String8 path = PushStr8F(scratch.arena, "%S\\vision_model.onnx", folder);
   if (ok && !FilePathExists(path)) {
      LogUserErrorF("No image model at %S\n", path);
      ok = 0;
   }

   ok = ok && _EMB_Ok(model, model->ort->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "videosearch", &model->env), "starting");
   ok = ok && _EMB_Ok(model, model->ort->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &model->memory),
                      "starting");

   if (ok) {
      OrtSessionOptions* options = 0;
      ok = _EMB_Ok(model, model->ort->CreateSessionOptions(&options), "starting");
      ok = ok && _EMB_Ok(model, model->ort->SetSessionGraphOptimizationLevel(options, ORT_ENABLE_ALL), "starting");

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
      ok = ok && _EMB_Ok(model, model->ort->CreateSession(model->env, wide, options, &model->image), "loading the image model");
      if (options != 0) model->ort->ReleaseSessionOptions(options);
   }

   // The input is the pictures, whatever it is named (pixel_values); the
   // output wanted is image_embeds, the vectors. The other is the model's
   // inner state.
   if (ok) {
      OrtAllocator* allocator = 0;
      ok = _EMB_Ok(model, model->ort->GetAllocatorWithDefaultOptions(&allocator), "reading the image model");
      char* name = 0;
      ok = ok && _EMB_Ok(model, model->ort->SessionGetInputName(model->image, 0, allocator, &name), "reading the image model");
      if (ok) {
         model->image_input = PushStr8Copy(arena, Str8CString(name));
         model->ort->AllocatorFree(allocator, name);
      }

      size_t output_count = 0;
      ok = ok && _EMB_Ok(model, model->ort->SessionGetOutputCount(model->image, &output_count), "reading the image model");
      String8List names = {0};
      for (size_t i = 0; ok && i < output_count; i += 1) {
         ok = _EMB_Ok(model, model->ort->SessionGetOutputName(model->image, i, allocator, &name), "reading the image model");
         if (ok) {
            String8 s = PushStr8Copy(scratch.arena, Str8CString(name));
            model->ort->AllocatorFree(allocator, name);
            Str8ListPush(scratch.arena, &names, s);
            if (Str8Match(s, Str8Lit("image_embeds"), 0)) {
               model->image_output = PushStr8Copy(arena, s);
            }
         }
      }
      if (ok && model->image_output.size == 0) {
         StringJoin join = {0};
         join.sep = Str8Lit(", ");
         LogUserErrorF("The image model has no output image_embeds, only: %S\n", Str8ListJoin(scratch.arena, &names, &join));
         ok = 0;
      }
   }

   if (!ok) {
      if (model->ort != 0) EMB_Close(model);
      model = 0;
   }
   ScratchEnd(scratch);
   return model;
}

internal B32
EMB_ImageVectors(EMB_Model* model, F32* images, U64 count, F32* vectors_out) {
   OrtValue* input = 0;
   OrtValue* output = 0;
   int64_t   shape[4] = {(int64_t)count, 3, EMB_IMAGE_DIM, EMB_IMAGE_DIM};
   B32       ok = _EMB_Ok(model,
                          model->ort->CreateTensorWithDataAsOrtValue(model->memory, images, count * EMB_IMAGE_FLOATS * sizeof(F32),
                                                                     shape, 4, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &input),
                          "encoding pictures");

   // The names were copied with their ending zero (PushStr8Copy).
   const char* input_name = (const char*)model->image_input.str;
   const char* output_name = (const char*)model->image_output.str;
   ok = ok && _EMB_Ok(model, model->ort->Run(model->image, 0, &input_name, (const OrtValue* const*)&input, 1, &output_name, 1, &output),
                      "encoding pictures");

   // The vectors are count by EMB_DIM. Checked, since the model is a file
   // that could be another one.
   F32* data = 0;
   if (ok) {
      OrtTensorTypeAndShapeInfo* info = 0;
      ok = _EMB_Ok(model, model->ort->GetTensorTypeAndShape(output, &info), "encoding pictures");
      size_t  dims_count = 0;
      int64_t dims[2] = {0};
      ok = ok && _EMB_Ok(model, model->ort->GetDimensionsCount(info, &dims_count), "encoding pictures");
      if (ok && dims_count == 2) {
         ok = _EMB_Ok(model, model->ort->GetDimensions(info, dims, 2), "encoding pictures");
      }
      if (info != 0) model->ort->ReleaseTensorTypeAndShapeInfo(info);
      if (ok && (dims_count != 2 || dims[0] != (int64_t)count || dims[1] != EMB_DIM)) {
         LogUserErrorF("The image model's vectors are not %llu by %d\n", (unsigned long long)count, EMB_DIM);
         ok = 0;
      }
      ok = ok && _EMB_Ok(model, model->ort->GetTensorMutableData(output, (void**)&data), "encoding pictures");
   }

   // Made length one.
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
   if (input != 0) model->ort->ReleaseValue(input);
   return ok;
}
