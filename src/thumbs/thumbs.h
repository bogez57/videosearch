////////////////////////////////
//~ bgz: thumbs: small pictures of moments in videos
//
// A frame of a video at a time, as a texture, for a hit's row. Asking for
// one that is not there starts it being made, on a thread of the layer's
// own (open the video small, seek, decode to the moment); it is there some
// frames later. The last THM_CAP asked for are kept.
//
// All of it is called from the main thread.

#ifndef THUMBS_H
#define THUMBS_H

// The longest side of a thumbnail, in pixels.
#define THM_DIM 192

// How many are kept. The one asked for longest ago goes when another is
// wanted.
#define THM_CAP 256

typedef struct THM_Thumbs THM_Thumbs;

typedef struct THM_Thumb THM_Thumb;
struct THM_Thumb {
   R_Handle texture; // zero: not there yet, or the video can't be decoded
   Vec2S32  size;
};

internal THM_Thumbs* THM_Alloc(void);

// Once a frame, before any THM_Get: makes textures of what the thread has
// decoded since.
internal void THM_Update(THM_Thumbs* thumbs);

// The video at path, at time_s (to the second). Good for this frame.
internal THM_Thumb THM_Get(THM_Thumbs* thumbs, String8 path, F64 time_s);

// Stops the thread, at the end of the program.
internal void THM_Stop(THM_Thumbs* thumbs);

#endif // THUMBS_H
