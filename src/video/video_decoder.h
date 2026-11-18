////////////////////////////////
//~ bgz: video: what the player asks of a decoder
//
// The player (video.c) is written against these functions and nothing
// else. A file that defines them is a decoder: video_ffmpeg.c is the one
// there is. To decode with something else, write another such file and
// include it in main.c in place of that one.
//
// A decoder is used by one thread at a time: the main thread opens and
// closes it, and between the two only the player's decode thread calls it.
// It does not have to be fast to seek or exact about it - the player throws
// away frames from before the time it asked for.

#ifndef VIDEO_DECODER_H
#define VIDEO_DECODER_H

typedef struct VID_Decoder VID_Decoder; // the decoder's own

typedef struct VID_DecoderInfo VID_DecoderInfo;
struct VID_DecoderInfo {
   Vec2S32 size;              // of the frames VID_DecoderNextFrame writes
   F64     duration_s;        // 0: not known
   F64     frames_per_second; // 0: not known
   B32     has_sound;         // VID_DecoderNextSound has something to give
};

// Opens the file. Frames come out no longer than max_dim on their longer
// side, made smaller if the video's own are larger. Sound, if the file has
// any and sound_rate is not 0, comes out as two channels at sound_rate
// frames a second, whatever the file's own is. What the decoder itself needs
// for as long as it is open may be pushed on arena. 0 if the file could not
// be opened as a video, and why is logged (LogUserErrorF).
internal VID_Decoder* VID_DecoderOpen(Arena* arena, String8 path, S32 max_dim, S32 sound_rate, VID_DecoderInfo* info_out);

// As VID_DecoderOpen, but the video is decoded on the graphics card if the
// card can decode it, and on the processor if not. For going through a
// whole video quickly with VID_DecoderNextFrameFrom: a frame that is not
// given out never leaves the card.
internal VID_Decoder* VID_DecoderOpenOnCard(Arena* arena, String8 path, S32 max_dim, S32 sound_rate, VID_DecoderInfo* info_out);
internal void         VID_DecoderClose(VID_Decoder* decoder);

// Decodes the next frame into pixels: info.size.x * info.size.y * 4 bytes,
// rows from the top, each pixel red, green, blue, alpha. time_s_out gets when
// it is shown, in seconds from the video's start. 0 at the end of the video,
// or when it can't be decoded further.
internal B32 VID_DecoderNextFrame(VID_Decoder* decoder, U8* pixels, F64* time_s_out);

// As VID_DecoderNextFrame, but gives out the first frame at or after
// from_s: the ones before it are decoded and let go, without being made
// into pixels.
internal B32 VID_DecoderNextFrameFrom(VID_Decoder* decoder, F64 from_s, U8* pixels, F64* time_s_out);

// Decodes the next of the video's sound into samples: up to frame_cap
// frames, each a left and a right F32. Returns how many, and time_s_out gets
// when the first of them is heard, on the same clock as the frames' times. 0
// at the end of the sound, or if there is none.
//
// Frames and sound are asked for separately and in any order: the decoder
// keeps what it has read of the one while it is asked for the other.
internal U64 VID_DecoderNextSound(VID_Decoder* decoder, F32* samples, U64 frame_cap, F64* time_s_out);

// The next frame and the next sound decoded will be at or before time_s - as near before
// it as the decoder can seek without decoding.
internal void VID_DecoderSeek(VID_Decoder* decoder, F64 time_s);

// What a file says of itself, for a program that cuts from it (the picks'
// timeline): read from the file's start, nothing decoded.
typedef struct VID_MediaInfo VID_MediaInfo;
struct VID_MediaInfo {
   Vec2S32 size;           // the picture's own, with pixels that are not square made so
   S32     rate_num;       // frames a second, as rate_num over rate_den (24000 over 1001). 0: not known
   S32     rate_den;
   F64     duration_s;     // 0: not known
   B32     has_sound;
   S32     sound_rate;     // samples a second
   S32     sound_channels;
   B32     has_timecode;   // the file says what its first frame's timecode is
   S32     timecode[4];    // hours, minutes, seconds, frames
   B32     timecode_drops; // it is drop-frame: written with a ; before the frames
};

// 0 if the file could not be opened as a video. Nothing is logged.
internal B32 VID_DecoderMediaInfo(String8 path, VID_MediaInfo* info_out);

#endif // VIDEO_DECODER_H
