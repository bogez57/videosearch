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
INX_FilePath(Arena* arena, String8 folder, String8 video_path, char* ext) {
   return PushStr8F(arena, "%S\\%016llx.%s", folder, (unsigned long long)U64HashFromStr8(video_path), ext);
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

internal B32
INX_Read(Arena* arena, String8 file_path, U64* count_out, F32** times_out, U16** vectors_out) {
   Temp    scratch = ScratchBegin(&arena, 1);
   String8 data = DataFromFilePath(scratch.arena, file_path);
   B32     result = 0;
   if (data.size >= sizeof(INX_Header)) {
      INX_Header header;
      MemoryCopy(&header, data.str, sizeof(header));
      U64 times_at = sizeof(header) + header.path_size;
      U64 vectors_at = times_at + header.count * sizeof(F32);
      U64 end = vectors_at + header.count * EMB_DIM * sizeof(U16);
      result = (MemoryMatch(header.magic, INX_MAGIC, sizeof(header.magic)) && header.version == INX_VERSION &&
                header.dim == EMB_DIM && end == data.size);
      if (result) {
         // Copied out, since after the path they may not be aligned.
         F32* times = PushArrayNoZero(arena, F32, header.count);
         U16* vectors = PushArrayNoZero(arena, U16, header.count * EMB_DIM);
         MemoryCopy(times, data.str + times_at, header.count * sizeof(F32));
         MemoryCopy(vectors, data.str + vectors_at, header.count * EMB_DIM * sizeof(U16));
         *count_out = header.count;
         *times_out = times;
         *vectors_out = vectors;
      }
   }
   ScratchEnd(scratch);
   return result;
}

internal F32
INX_F32FromF16(U16 h) {
   U32 sign = (U32)(h & 0x8000) << 16;
   U32 exp = (h >> 10) & 0x1F;
   U32 mant = h & 0x3FF;
   U32 x = 0;
   if (exp == 0x1F) {
      x = sign | 0x7F800000 | (mant << 13); // infinity, or not a number
   } else if (exp != 0) {
      x = sign | ((exp + 127 - 15) << 23) | (mant << 13);
   } else if (mant != 0) {
      // A subnormal F16 is a normal F32: shift it up until its top bit is
      // the hidden one.
      S32 e = 127 - 15 + 1;
      while ((mant & 0x400) == 0) {
         mant <<= 1;
         e -= 1;
      }
      x = sign | ((U32)e << 23) | ((mant & 0x3FF) << 13);
   } else {
      x = sign;
   }
   F32 result;
   MemoryCopy(&result, &x, 4);
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
