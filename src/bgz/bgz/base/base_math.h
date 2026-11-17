////////////////////////////////
//~ bgz: Math
//
// bgz's functions for raddebugger's math (rad/base/base_math.h). Each only
// calls raddebugger's function of the same name.

#ifndef BGZ_BASE_MATH_H
#define BGZ_BASE_MATH_H

exported Vec2F32 V2F32(F32 x, F32 y);
exported Vec2S16 V2S16(S16 x, S16 y);
exported Vec2S32 V2S32(S32 x, S32 y);
exported Vec2S64 V2S64(S64 x, S64 y);
exported Vec4F32 V4F32(F32 x, F32 y, F32 z, F32 w);

exported Vec2F32 Add2F32(Vec2F32 a, Vec2F32 b);
exported Vec2F32 Sub2F32(Vec2F32 a, Vec2F32 b);
exported F32     Length2F32(Vec2F32 v);

exported Rng1F32 R1F32(F32 min, F32 max);
exported Rng1S64 R1S64(S64 min, S64 max);
exported Rng1U64 R1U64(U64 min, U64 max);
exported Rng2F32 R2F32(Vec2F32 min, Vec2F32 max);
exported Rng2S32 R2S32(Vec2S32 min, Vec2S32 max);

exported S64     Dim1S64(Rng1S64 r);
exported U64     Dim1U64(Rng1U64 r);
exported S64     Clamp1S64(Rng1S64 r, S64 v);

exported Rng2F32 Shift2F32(Rng2F32 r, Vec2F32 x);
exported Rng2F32 Pad2F32(Rng2F32 r, F32 x);
exported Vec2F32 Center2F32(Rng2F32 r);
exported B32     Contains2F32(Rng2F32 r, Vec2F32 x);
exported Vec2F32 Dim2F32(Rng2F32 r);
exported Rng2F32 Union2F32(Rng2F32 a, Rng2F32 b);
exported Rng2F32 Intersect2F32(Rng2F32 a, Rng2F32 b);

exported Rng1U64 MRangeFromNIdxMCount(U64 n_idx, U64 n_count, U64 m_count);

#define AbsF32(v)    abs_f32(v)
#define CeilF32(v)   ceil_f32(v)
#define FloorF32(v)  floor_f32(v)
#define RoundF32(v)  round_f32(v)
#define ModF32(a, b) mod_f32((a), (b))
#define PowF32(b, e) pow_f32((b), (e))

#define R2F32P(x, y, z, w) R2F32(V2F32((x), (y)), V2F32((z), (w)))

#endif // BGZ_BASE_MATH_H
