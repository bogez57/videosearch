////////////////////////////////
//~ bgz: embed: getting a picture ready for the model
//
// See embed.h. As CLIP's own preprocessing (preprocessor_config.json in the
// model's folder): the shorter side scaled to 224, the middle 224x224 cut
// out, each colour made 0 to 1 and then normalized with the mean and spread
// of the pictures CLIP was trained on.
//
// CLIP scales with a bicubic filter; this is bilinear, done in the same pass
// as the cut. The frames come from the decoder a little larger than 224 on
// their shorter side (IDX_FRAME_DIM), so they are only scaled down a little
// and the two come out nearly the same.

global F32 _emb_mean[3] = {0.48145466f, 0.4578275f, 0.40821073f};
global F32 _emb_std[3] = {0.26862954f, 0.26130258f, 0.27577711f};

internal void
EMB_ImageFromFrame(U8* pixels, Vec2S32 size, F32* image_out) {
   S32 side = Min(size.x, size.y);
   F32 scale = (F32)side / (F32)EMB_IMAGE_DIM;
   F32 off_x = (F32)(size.x - side) * 0.5f;
   F32 off_y = (F32)(size.y - side) * 0.5f;
   U64 plane = (U64)EMB_IMAGE_DIM * EMB_IMAGE_DIM;

   for (S32 y = 0; y < EMB_IMAGE_DIM; y += 1) {
      // Where the middle of this pixel falls in the frame, between which rows.
      F32 sy = off_y + ((F32)y + 0.5f) * scale - 0.5f;
      sy = Clamp(0.0f, sy, (F32)(size.y - 1));
      S32 y0 = (S32)sy;
      S32 y1 = Min(y0 + 1, size.y - 1);
      F32 fy = sy - (F32)y0;

      for (S32 x = 0; x < EMB_IMAGE_DIM; x += 1) {
         F32 sx = off_x + ((F32)x + 0.5f) * scale - 0.5f;
         sx = Clamp(0.0f, sx, (F32)(size.x - 1));
         S32 x0 = (S32)sx;
         S32 x1 = Min(x0 + 1, size.x - 1);
         F32 fx = sx - (F32)x0;

         U8* p00 = pixels + ((U64)y0 * (U64)size.x + (U64)x0) * 4;
         U8* p01 = pixels + ((U64)y0 * (U64)size.x + (U64)x1) * 4;
         U8* p10 = pixels + ((U64)y1 * (U64)size.x + (U64)x0) * 4;
         U8* p11 = pixels + ((U64)y1 * (U64)size.x + (U64)x1) * 4;
         U64 out = (U64)y * EMB_IMAGE_DIM + (U64)x;
         for (U64 c = 0; c < 3; c += 1) {
            F32 top = (F32)p00[c] + ((F32)p01[c] - (F32)p00[c]) * fx;
            F32 bottom = (F32)p10[c] + ((F32)p11[c] - (F32)p10[c]) * fx;
            F32 v = (top + (bottom - top) * fy) / 255.0f;
            image_out[c * plane + out] = (v - _emb_mean[c]) / _emb_std[c];
         }
      }
   }
}
