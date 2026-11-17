////////////////////////////////
//~ bgz: Vectors

exported Vec2F32
V2F32(F32 x, F32 y) {
   Vec2F32 result = vec_2f32(x, y);
   return result;
}

exported Vec2S16
V2S16(S16 x, S16 y) {
   Vec2S16 result = vec_2s16(x, y);
   return result;
}

exported Vec2S32
V2S32(S32 x, S32 y) {
   Vec2S32 result = vec_2s32(x, y);
   return result;
}

exported Vec2S64
V2S64(S64 x, S64 y) {
   Vec2S64 result = vec_2s64(x, y);
   return result;
}

exported Vec4F32
V4F32(F32 x, F32 y, F32 z, F32 w) {
   Vec4F32 result = vec_4f32(x, y, z, w);
   return result;
}

exported Vec2F32
Add2F32(Vec2F32 a, Vec2F32 b) {
   Vec2F32 result = add_2f32(a, b);
   return result;
}

exported Vec2F32
Sub2F32(Vec2F32 a, Vec2F32 b) {
   Vec2F32 result = sub_2f32(a, b);
   return result;
}

exported F32
Length2F32(Vec2F32 v) {
   F32 result = length_2f32(v);
   return result;
}

////////////////////////////////
//~ bgz: Ranges

exported Rng1F32
R1F32(F32 min, F32 max) {
   Rng1F32 result = rng_1f32(min, max);
   return result;
}

exported Rng1S64
R1S64(S64 min, S64 max) {
   Rng1S64 result = rng_1s64(min, max);
   return result;
}

exported Rng1U64
R1U64(U64 min, U64 max) {
   Rng1U64 result = rng_1u64(min, max);
   return result;
}

exported Rng2F32
R2F32(Vec2F32 min, Vec2F32 max) {
   Rng2F32 result = rng_2f32(min, max);
   return result;
}

exported Rng2S32
R2S32(Vec2S32 min, Vec2S32 max) {
   Rng2S32 result = rng_2s32(min, max);
   return result;
}

exported S64
Dim1S64(Rng1S64 r) {
   S64 result = dim_1s64(r);
   return result;
}

exported U64
Dim1U64(Rng1U64 r) {
   U64 result = dim_1u64(r);
   return result;
}

exported S64
Clamp1S64(Rng1S64 r, S64 v) {
   S64 result = clamp_1s64(r, v);
   return result;
}

exported Rng2F32
Shift2F32(Rng2F32 r, Vec2F32 x) {
   Rng2F32 result = shift_2f32(r, x);
   return result;
}

exported Rng2F32
Pad2F32(Rng2F32 r, F32 x) {
   Rng2F32 result = pad_2f32(r, x);
   return result;
}

exported Vec2F32
Center2F32(Rng2F32 r) {
   Vec2F32 result = center_2f32(r);
   return result;
}

exported B32
Contains2F32(Rng2F32 r, Vec2F32 x) {
   B32 result = contains_2f32(r, x);
   return result;
}

exported Vec2F32
Dim2F32(Rng2F32 r) {
   Vec2F32 result = dim_2f32(r);
   return result;
}

exported Rng2F32
Union2F32(Rng2F32 a, Rng2F32 b) {
   Rng2F32 result = union_2f32(a, b);
   return result;
}

exported Rng2F32
Intersect2F32(Rng2F32 a, Rng2F32 b) {
   Rng2F32 result = intersect_2f32(a, b);
   return result;
}

////////////////////////////////
//~ bgz: Miscellaneous

exported Rng1U64
MRangeFromNIdxMCount(U64 n_idx, U64 n_count, U64 m_count) {
   Rng1U64 result = m_range_from_n_idx_m_count(n_idx, n_count, m_count);
   return result;
}
