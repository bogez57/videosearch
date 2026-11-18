////////////////////////////////
//~ bgz: embed: getting a picture ready for the model
//
// See embed.h. As the model's own preprocessing (preprocessor_config.json in
// its folder): scaled to 224 with a bilinear filter, each colour made 0 to
// 1, and nothing more. What is scaled is the part of the frame the view
// shows.
//
// Where the frame is made smaller, each pixel of the square is made of all
// the frame's pixels it covers, not only the nearest two: the filter is as
// wide as the scale, as it is in Pillow, which the model's makers scaled
// with. A whole 16:9 frame pressed into the square is more than halved
// across, and without that every other column of it would be skipped.

// For each of the square's pixels along one side, the pixels of the frame it
// is made of: the first, and how much of each of the taps from there (a
// weight is 0 past the frame's edge).
typedef struct _EMB_Taps _EMB_Taps;
struct _EMB_Taps {
   S32  taps;
   S32  first[EMB_IMAGE_DIM];
   F32* weights; // EMB_IMAGE_DIM by taps
};

// length of the frame's side, starting at from, onto the square's side. limit
// is the whole side.
internal _EMB_Taps
_EMB_TapsFor(Arena* arena, F32 from, F32 length, S32 limit) {
   _EMB_Taps result = {0};
   F32       scale = length / (F32)EMB_IMAGE_DIM;
   F32       reach = Max(scale, 1.0f); // how far each way the filter goes, in the frame's pixels
   result.taps = (S32)ceilf(2.0f * reach) + 1;
   result.weights = PushArray(arena, F32, (U64)EMB_IMAGE_DIM * (U64)result.taps);
   for (S32 i = 0; i < EMB_IMAGE_DIM; i += 1) {
      F32  center = from + ((F32)i + 0.5f) * scale;
      S32  first = Max(0, (S32)floorf(center - reach + 0.5f));
      S32  end = Min(limit, (S32)floorf(center + reach + 0.5f));
      F32* w = result.weights + (U64)i * (U64)result.taps;
      F32  sum = 0;
      for (S32 k = 0; k < result.taps && first + k < end; k += 1) {
         w[k] = Max(0.0f, 1.0f - fabsf(((F32)(first + k) + 0.5f - center) / reach));
         sum += w[k];
      }
      for (S32 k = 0; k < result.taps && sum > 0; k += 1) w[k] /= sum;
      result.first[i] = first;
   }
   return result;
}

internal void
EMB_ImageFromFrame(U8* pixels, Vec2S32 size, EMB_View view, F32* image_out) {
   Temp scratch = ScratchBegin(0, 0);

   // The part of the frame the view shows: all of it, or a square as high
   // as the frame's shorter side, from one end of the longer side, its
   // middle or its other end.
   F32 x0 = 0;
   F32 y0 = 0;
   F32 w = (F32)size.x;
   F32 h = (F32)size.y;
   if (view != EMB_View_Whole) {
      F32 side = Min(w, h);
      F32 along = (view == EMB_View_Start) ? 0.0f : (view == EMB_View_Middle) ? 0.5f : 1.0f;
      x0 = (w - side) * along;
      y0 = (h - side) * along;
      w = side;
      h = side;
   }
   _EMB_Taps across = _EMB_TapsFor(scratch.arena, x0, w, size.x);
   _EMB_Taps down = _EMB_TapsFor(scratch.arena, y0, h, size.y);

   // Across first: each row of the frame that is used, into a row as wide
   // as the square, red green blue.
   S32  row0 = down.first[0];
   S32  row1 = Min(size.y, down.first[EMB_IMAGE_DIM - 1] + down.taps);
   F32* rows = PushArrayNoZero(scratch.arena, F32, (U64)(row1 - row0) * EMB_IMAGE_DIM * 3);
   for (S32 y = row0; y < row1; y += 1) {
      U8*  in = pixels + (U64)y * (U64)size.x * 4;
      F32* out = rows + (U64)(y - row0) * EMB_IMAGE_DIM * 3;
      for (S32 x = 0; x < EMB_IMAGE_DIM; x += 1) {
         F32* wx = across.weights + (U64)x * (U64)across.taps;
         F32  rgb[3] = {0};
         for (S32 k = 0; k < across.taps && across.first[x] + k < size.x; k += 1) {
            U8* p = in + (U64)(across.first[x] + k) * 4;
            rgb[0] += wx[k] * (F32)p[0];
            rgb[1] += wx[k] * (F32)p[1];
            rgb[2] += wx[k] * (F32)p[2];
         }
         out[x * 3 + 0] = rgb[0];
         out[x * 3 + 1] = rgb[1];
         out[x * 3 + 2] = rgb[2];
      }
   }

   // Then down, into the three planes.
   U64 plane = (U64)EMB_IMAGE_DIM * EMB_IMAGE_DIM;
   for (S32 y = 0; y < EMB_IMAGE_DIM; y += 1) {
      F32* wy = down.weights + (U64)y * (U64)down.taps;
      F32* out = image_out + (U64)y * EMB_IMAGE_DIM;
      for (S32 x = 0; x < EMB_IMAGE_DIM; x += 1) {
         F32 rgb[3] = {0};
         for (S32 k = 0; k < down.taps && down.first[y] + k < row1; k += 1) {
            F32* p = rows + ((U64)(down.first[y] + k - row0) * EMB_IMAGE_DIM + (U64)x) * 3;
            rgb[0] += wy[k] * p[0];
            rgb[1] += wy[k] * p[1];
            rgb[2] += wy[k] * p[2];
         }
         out[x] = rgb[0] / 255.0f;
         out[plane + x] = rgb[1] / 255.0f;
         out[2 * plane + x] = rgb[2] / 255.0f;
      }
   }
   ScratchEnd(scratch);
}
