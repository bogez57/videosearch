////////////////////////////////
//~ bgz: index
//
// See index.h.

internal INX_Header
INX_HeaderFor(String8 video_path, FileProperties video, String8 model, F64 sample_seconds) {
   INX_Header result = {0};
   MemoryCopy(result.magic, INX_MAGIC, sizeof(result.magic));
   result.version = INX_VERSION;
   result.dim = EMB_DIM;
   result.video_size = video.size;
   result.video_modified = video.modified;
   result.sample_seconds = sample_seconds;
   MemoryCopy(result.model, model.str, Min(model.size, sizeof(result.model) - 1));
   result.path_size = video_path.size;
   return result;
}

internal String8
INX_FilePath(Arena* arena, String8 folder, String8 video_path) {
   return PushStr8F(arena, "%S\\%016llx.vsi", folder, (unsigned long long)U64HashFromStr8(video_path));
}

internal B32
INX_IsUpToDate(String8 file_path, INX_Header* want, U64* count_out) {
   Temp scratch = ScratchBegin(0, 0);
   B32  result = 0;
   File file = FileOpen(AccessFlag_Read | AccessFlag_ShareRead, file_path);
   if (!FileMatch(file, FileZero())) {
      String8 got = StringFromFileRange(scratch.arena, file, R1U64(0, sizeof(INX_Header)));
      if (got.size == sizeof(INX_Header)) {
         INX_Header have = *(INX_Header*)got.str;
         U64        count = have.count;
         have.count = want->count;
         result = MemoryMatch(&have, want, sizeof(INX_Header));
         *count_out = count;
      }
      FileClose(file);
   }
   ScratchEnd(scratch);
   return result;
}

internal B32
INX_Write(String8 file_path, INX_Header header, String8 video_path, String8List times, String8List vectors) {
   Temp scratch = ScratchBegin(0, 0);
   header.count = times.total_size / sizeof(F32);

   String8List parts = {0};
   Str8ListPush(scratch.arena, &parts, Str8((U8*)&header, sizeof(header)));
   Str8ListPush(scratch.arena, &parts, video_path);
   for (String8Node* n = times.first; n != 0; n = n->next) Str8ListPush(scratch.arena, &parts, n->string);
   for (String8Node* n = vectors.first; n != 0; n = n->next) Str8ListPush(scratch.arena, &parts, n->string);

   String8 temp_path = PushStr8F(scratch.arena, "%S.part", file_path);
   B32     result = 0;
   File    file = FileOpen(AccessFlag_Write, temp_path);
   if (!FileMatch(file, FileZero())) {
      result = (FileWriteList(file, 0, parts) == parts.total_size);
      FileClose(file);
      result = result && ReplaceFilePath(file_path, temp_path);
      if (!result) {
         DeleteFileAtPath(temp_path);
      }
   }
   ScratchEnd(scratch);
   return result;
}

internal U16
INX_F16FromF32(F32 f) {
   U32 x;
   MemoryCopy(&x, &f, 4);
   U32 sign = (x >> 16) & 0x8000;
   S32 exp = (S32)((x >> 23) & 0xFF) - 127 + 15;
   U32 mant = x & 0x7FFFFF;
   U16 result = 0;
   if (((x >> 23) & 0xFF) == 0xFF) {
      result = (U16)(sign | 0x7C00 | (mant ? 0x200 : 0)); // infinity, or not a number
   } else if (exp >= 31) {
      result = (U16)(sign | 0x7C00);                       // too large: infinity
   } else if (exp <= 0) {
      // Too small for F16's normal numbers: a subnormal one, or zero.
      if (exp >= -10) {
         mant |= 0x800000;
         U32 shift = (U32)(14 - exp);
         U32 half = mant >> shift;
         U32 rest = mant & ((1u << shift) - 1);
         U32 mid = 1u << (shift - 1);
         if (rest > mid || (rest == mid && (half & 1))) half += 1;
         result = (U16)(sign | half);
      } else {
         result = (U16)sign;
      }
   } else {
      U32 half = ((U32)exp << 10) | (mant >> 13);
      U32 rest = mant & 0x1FFF;
      if (rest > 0x1000 || (rest == 0x1000 && (half & 1))) half += 1; // a carry into the exponent is right
      result = (U16)(sign | half);
   }
   return result;
}
