////////////////////////////////
//~ bgz: video
//
// A video file, played into a texture: what a slide draws where a picture
// would go, with its sound through the mixer (src\audio).
//
// The layer is in two parts, so that what decodes can be changed without
// anything else knowing (video.md has the whole of it):
//
//    video.h, video.c       the player. A thread that decodes ahead, a few
//                           frames waiting, a clock, and the texture. Knows
//                           nothing of file formats or codecs.
//    video_decoder.h        what the player asks of a decoder: open, next
//                           frame, next sound, seek, close.
//    video_ffmpeg.c         those, done with FFmpeg. The only file
//                           that knows FFmpeg is there.
//
// The rest of the program uses only what is declared in this file.
//
// All of it is called from the main thread. Times are in seconds from the
// video's start.

#ifndef VIDEO_H
#define VIDEO_H

// The longest side a video is decoded at. A larger one is made smaller as
// it is decoded: a slide never shows more, and a frame of 4K is 33 MB to
// convert, keep and send to the renderer 60 times a second.
#define VID_MAX_DIM 1920

typedef struct VID_Video VID_Video;

// Whether a file name ends in an extension that is a video's.
internal B32 VID_NameIsVideo(String8 name);

// Opens the file and starts decoding it. The video is paused at its start,
// and its first frame is in the texture by the first or second VID_Update.
// 0 if it could not be opened, and why is logged.
internal VID_Video* VID_Open(String8 path);
internal void       VID_Close(VID_Video* video);

// Black until the first frame is in. As long as the video is open.
internal R_Handle VID_Texture(VID_Video* video);
internal Vec2S32  VID_Size(VID_Video* video); // of the texture, in pixels

// 0 if the file does not say.
internal F64 VID_Duration(VID_Video* video);
internal F64 VID_Position(VID_Video* video);

// Playing a video that is at its end starts it over.
internal B32  VID_IsPlaying(VID_Video* video);
internal void VID_SetPlaying(VID_Video* video, B32 playing);
internal void VID_Seek(VID_Video* video, F64 time_s);
// Whether it has played to its end and stopped there.
internal B32  VID_HasEnded(VID_Video* video);

// No sound from this one. It keeps its time by its sound all the same.
internal void VID_SetMuted(VID_Video* video, B32 muted);

// Once a frame: moves the clock on and puts the frame that is due in the
// texture. A video that reaches its end stops there, on its last frame.
internal void VID_Update(VID_Video* video);

#endif // VIDEO_H
