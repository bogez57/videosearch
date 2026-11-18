////////////////////////////////
//~ bgz: video: the decoder, done with FFmpeg
//
// The functions of video_decoder.h. This is the only file in the
// program that includes FFmpeg's headers or calls it: to decode with
// something else, write another file with the same functions and
// include that one in main.c instead.
//
// FFmpeg is src\third_party\ffmpeg: its headers, the import libraries the
// program links against, and the DLLs, which build.bat copies next to the
// exe. It is the LGPL build, used as it comes: nothing of it is compiled
// into the program.
//
//    libavformat   opens the file and hands out its packets
//    libavcodec    decodes the video stream's packets into frames
//    libswscale    turns a frame, in whatever form the codec has it, into
//                  RGBA at the size asked for
//    libswresample turns sound, in whatever form its codec has it, into two
//                  channels of F32 at the rate asked for
//    libavutil     what the three share
//
// Decoding is on the processor, on as many threads as FFmpeg sees fit.
// VID_DecoderOpenOnCard asks for the graphics card's decoder instead
// (Direct3D 11's, which any card's driver has): FFmpeg hands the codec the
// card, the frames come out as textures on it, and one is brought back to
// memory only when its pixels are wanted. A video the card can't decode is
// decoded on the processor, by FFmpeg's own choice (_VID_PickFormat).
//
// FFmpeg's memory is its own (malloc): its libraries are DLLs with no way
// to be given an arena. Everything it allocates here is let go of in
// VID_DecoderClose.

// FFmpeg's headers are found through -Isrc\third_party\ffmpeg\include
// (build.bat): they include each other from there. They have fields named
// internal and global, which here are words for static: put aside while the
// headers are read.
#pragma push_macro("internal")
#pragma push_macro("global")
#undef internal
#undef global
#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Weverything"
#endif
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libavutil/error.h"
#include "libavutil/hwcontext.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/channel_layout.h"
#include "libavutil/pixdesc.h"
#if defined(__clang__)
# pragma clang diagnostic pop
#endif
#pragma pop_macro("global")
#pragma pop_macro("internal")

// Packets of the file read while looking for the other stream's: a file has
// its video and its sound in turns, and the one asked for is rarely the
// one that comes next.
typedef struct _VID_PacketNode _VID_PacketNode;
struct _VID_PacketNode {
   _VID_PacketNode* next;
   AVPacket*        packet;
};

typedef struct _VID_PacketQueue _VID_PacketQueue;
struct _VID_PacketQueue {
   _VID_PacketNode* first;
   _VID_PacketNode* last;
};

struct VID_Decoder {
   String8          path; // to say which file, when something goes wrong
   AVFormatContext* format;
   AVCodecContext*  codec;
   int              stream_idx;
   AVPacket*        packet;
   AVFrame*         frame;
   struct SwsContext* scale;

   // Decoding on the graphics card: the card, and a frame brought back from
   // it. Both 0 when it was not asked for or there is no card to ask.
   AVBufferRef* card;
   AVFrame*     card_frame;

   Vec2S32 size;         // of the frames given out
   F64     seconds_per_tick; // the stream's time base
   S64     start_ticks;  // the time of the stream's first frame, which is given out as 0
   F64     frame_s;      // how long a frame lasts, if the file says. Else 0
   F64     last_time_s;  // of the last frame given out
   B32     is_draining;  // the file's packets are all read: the codec is giving up what it holds

   // Since the last seek: how many frames came out, and whether it was to
   // the very start. A seek that lands past a file's last keyframe before
   // the time - a .ts with one keyframe at its start does this - gives no
   // frame at all, and is tried again from the start.
   U64 frames_since_seek;
   B32 seek_was_to_start;

   //- bgz: reading the file for two streams
   B32              file_ended; // there are no more packets to read
   _VID_PacketQueue video_packets;
   _VID_PacketQueue sound_packets;

   //- bgz: sound. sound_codec is 0 if the file has none, or none was asked for
   int                sound_stream_idx;
   AVCodecContext*    sound_codec;
   AVFrame*           sound_frame;
   struct SwrContext* sound_convert;
   S32                sound_rate;             // of what is given out
   int                sound_in_rate;          // what sound_convert was made for:
   int                sound_in_format;        // made again when the file's sound
   int                sound_in_channels;      // changes form part way through
   F64                sound_seconds_per_tick;
   F64                sound_zero_s;           // the video's start, which is time 0 for the sound too
   B32                sound_is_draining;
   F64                sound_next_s;           // when the sound after the last decoded is heard

   // Decoded sound not given out yet: a frame of the file's sound is
   // usually more than is asked for at once.
   F32* sound_pending;       // FFmpeg's memory (av_realloc)
   U64  sound_pending_cap;   // in frames
   U64  sound_pending_first;
   U64  sound_pending_count;
   F64  sound_pending_time_s; // of the first not given out
};

internal void
_VID_PacketQueueClear(_VID_PacketQueue* queue) {
   for (_VID_PacketNode* node = queue->first, *next = 0; node != 0; node = next) {
      next = node->next;
      av_packet_free(&node->packet);
      av_free(node);
   }
   queue->first = 0;
   queue->last = 0;
}

// The next packet of the video (want_sound 0) or of the sound, into
// decoder->packet, which the caller unrefs. 0 when there are no more.
// Packets of the other stream met on the way are kept for when it asks.
internal B32
_VID_NextPacket(VID_Decoder* decoder, B32 want_sound) {
   B32               result = 0;
   _VID_PacketQueue* mine = want_sound ? &decoder->sound_packets : &decoder->video_packets;
   _VID_PacketQueue* others = want_sound ? &decoder->video_packets : &decoder->sound_packets;
   int               my_idx = want_sound ? decoder->sound_stream_idx : decoder->stream_idx;
   int               other_idx = want_sound ? decoder->stream_idx : (decoder->sound_codec != 0 ? decoder->sound_stream_idx : -1);

   if (mine->first != 0) {
      _VID_PacketNode* node = mine->first;
      mine->first = node->next;
      if (mine->first == 0) {
         mine->last = 0;
      }
      av_packet_unref(decoder->packet);
      av_packet_move_ref(decoder->packet, node->packet);
      av_packet_free(&node->packet);
      av_free(node);
      result = 1;
   }

   while (!result && !decoder->file_ended) {
      if (av_read_frame(decoder->format, decoder->packet) < 0) {
         // The end of the file - or it can't be read further, which is the
         // end of what can be played.
         decoder->file_ended = 1;
         break;
      }
      if (decoder->packet->stream_index == my_idx) {
         result = 1;
      } else if (decoder->packet->stream_index == other_idx) {
         _VID_PacketNode* node = (_VID_PacketNode*)av_mallocz(sizeof(_VID_PacketNode));
         AVPacket*        kept = av_packet_alloc();
         if (node != 0 && kept != 0) {
            av_packet_move_ref(kept, decoder->packet);
            node->packet = kept;
            if (others->last != 0) {
               others->last->next = node;
            } else {
               others->first = node;
            }
            others->last = node;
         } else {
            av_free(node);
            av_packet_free(&kept);
            av_packet_unref(decoder->packet);
         }
      } else {
         av_packet_unref(decoder->packet);
      }
   }

   return result;
}

// After the file has been sought in: what was read and decoded from where
// it was is let go of, for the video and the sound both.
internal void
_VID_AfterSeek(VID_Decoder* decoder) {
   _VID_PacketQueueClear(&decoder->video_packets);
   _VID_PacketQueueClear(&decoder->sound_packets);
   decoder->file_ended = 0;

   avcodec_flush_buffers(decoder->codec);
   decoder->is_draining = 0;
   decoder->frames_since_seek = 0;

   if (decoder->sound_codec != 0) {
      avcodec_flush_buffers(decoder->sound_codec);
      decoder->sound_is_draining = 0;
      decoder->sound_pending_count = 0;
      // Made again at the next sound: it holds a little of the old.
      swr_free(&decoder->sound_convert);
   }
}

// To the first byte of the file, which is always before the first
// keyframe, for the formats that are a plain run of packets (.ts, .mpg).
// Else to the first frame's time: a format with an index of its own - .mkv
// - takes a seek to a byte without saying no, and then reads from wherever
// it was.
internal void
_VID_SeekToStart(VID_Decoder* decoder) {
   int error = -1;
   int format_flags = decoder->format->iformat->flags;
   if ((format_flags & AVFMT_TS_DISCONT) && !(format_flags & AVFMT_NO_BYTE_SEEK)) {
      error = av_seek_frame(decoder->format, decoder->stream_idx, 0, AVSEEK_FLAG_BYTE);
   }
   if (error < 0) {
      av_seek_frame(decoder->format, decoder->stream_idx, decoder->start_ticks, AVSEEK_FLAG_BACKWARD);
   }
   _VID_AfterSeek(decoder);
   decoder->seek_was_to_start = 1;
   decoder->sound_next_s = 0;
}

// What FFmpeg has to say about one of its error codes.
internal String8
_VID_FFmpegError(Arena* arena, int code) {
   char text[AV_ERROR_MAX_STRING_SIZE] = {0};
   av_strerror(code, text, sizeof(text));
   String8 result = PushStr8Copy(arena, Str8CString(text));
   return result;
}

internal void
VID_DecoderClose(VID_Decoder* decoder) {
   if (decoder == 0) {
      return;
   }
   // Each of these takes a pointer to nothing in its stride.
   _VID_PacketQueueClear(&decoder->video_packets);
   _VID_PacketQueueClear(&decoder->sound_packets);
   swr_free(&decoder->sound_convert);
   av_freep(&decoder->sound_pending);
   av_frame_free(&decoder->sound_frame);
   avcodec_free_context(&decoder->sound_codec);
   sws_freeContext(decoder->scale);
   av_frame_free(&decoder->frame);
   av_frame_free(&decoder->card_frame);
   av_packet_free(&decoder->packet);
   avcodec_free_context(&decoder->codec);
   av_buffer_unref(&decoder->card);
   avformat_close_input(&decoder->format);
}

// FFmpeg asks which of the forms a codec can decode to is wanted: the
// card's, if it is among them. If the card then can't decode this video,
// FFmpeg asks again without it, and the first of the rest is one the
// processor decodes to.
internal enum AVPixelFormat
_VID_PickFormat(AVCodecContext* codec, const enum AVPixelFormat* formats) {
   enum AVPixelFormat result = formats[0];
   for (const enum AVPixelFormat* f = formats; *f != AV_PIX_FMT_NONE; f += 1) {
      if (*f == AV_PIX_FMT_D3D11) {
         result = *f;
         break;
      }
   }
   // Not the card's: the first that is no card's at all.
   if (result != AV_PIX_FMT_D3D11) {
      for (const enum AVPixelFormat* f = formats; *f != AV_PIX_FMT_NONE; f += 1) {
         const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(*f);
         if (desc != 0 && !(desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) {
            result = *f;
            break;
         }
      }
   }
   UnusedVariable(codec);
   return result;
}

internal VID_Decoder*
_VID_DecoderOpen(Arena* arena, String8 path, S32 max_dim, S32 sound_rate, VID_DecoderInfo* info_out, B32 on_card) {
   VID_Decoder* decoder = PushArray(arena, VID_Decoder, 1);
   decoder->path = PushStr8Copy(arena, path);
   decoder->stream_idx = -1;
   decoder->sound_stream_idx = -1;
   decoder->seek_was_to_start = 1; // a file opened is at its start

   // Only what goes wrong, to the console. Why a file can't be played is
   // logged below, for the program's own log.
   av_log_set_level(AV_LOG_ERROR);

   B32  is_open = 0;
   Temp scratch = ScratchBegin(&arena, 1);

   // FFmpeg takes the path as UTF-8, ended with a 0, on Windows too.
   String8 path_z = PushStr8Copy(scratch.arena, path);

   const AVCodec* codec = 0;
   int            error = avformat_open_input(&decoder->format, (char*)path_z.str, 0, 0);
   if (error < 0) {
      LogUserErrorF("Could not open %S as a video: %S\n", path, _VID_FFmpegError(scratch.arena, error));
   } else if ((error = avformat_find_stream_info(decoder->format, 0)) < 0) {
      LogUserErrorF("Could not read what is in %S: %S\n", path, _VID_FFmpegError(scratch.arena, error));
   } else if ((decoder->stream_idx = av_find_best_stream(decoder->format, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0)) < 0 ||
              codec == 0) {
      LogUserErrorF("%S has no video in it that can be decoded\n", path);
   } else {
      AVStream* stream = decoder->format->streams[decoder->stream_idx];

      decoder->codec = avcodec_alloc_context3(codec);
      if (decoder->codec == 0) {
         LogUserErrorF("Out of memory opening %S\n", path);
      } else if ((error = avcodec_parameters_to_context(decoder->codec, stream->codecpar)) < 0) {
         LogUserErrorF("Could not set up the decoder for %S: %S\n", path, _VID_FFmpegError(scratch.arena, error));
      } else {
         // 0: as many threads as it sees fit.
         decoder->codec->thread_count = 0;

         // The card, if it was asked for, there is one, and this codec has a
         // decoder on it. Without, it is decoded on the processor as ever.
         if (on_card) {
            B32 codec_has_card = 0;
            for (int i = 0;; i += 1) {
               const AVCodecHWConfig* config = avcodec_get_hw_config(codec, i);
               if (config == 0) break;
               if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) &&
                   config->device_type == AV_HWDEVICE_TYPE_D3D11VA) {
                  codec_has_card = 1;
                  break;
               }
            }
            if (codec_has_card && av_hwdevice_ctx_create(&decoder->card, AV_HWDEVICE_TYPE_D3D11VA, 0, 0, 0) >= 0) {
               decoder->codec->hw_device_ctx = av_buffer_ref(decoder->card);
               decoder->codec->get_format = _VID_PickFormat;
               decoder->card_frame = av_frame_alloc();
            }
         }
         error = avcodec_open2(decoder->codec, codec, 0);
         if (error < 0) {
            LogUserErrorF("Could not start the %s decoder for %S: %S\n", codec->name, path,
                          _VID_FFmpegError(scratch.arena, error));
         } else if (decoder->codec->width <= 0 || decoder->codec->height <= 0) {
            LogUserErrorF("%S does not say how big its picture is\n", path);
         } else {
            decoder->packet = av_packet_alloc();
            decoder->frame = av_frame_alloc();
            is_open = (decoder->packet != 0 && decoder->frame != 0);
            if (!is_open) {
               LogUserErrorF("Out of memory opening %S\n", path);
            }
         }
      }

      if (is_open) {
         //- bgz: how big the frames come out
         //
         // Pixels that are not square are made so: the width is the one
         // changed. Then the whole is made smaller if it is over max_dim.
         F64        width = (F64)decoder->codec->width;
         F64        height = (F64)decoder->codec->height;
         AVRational pixel_shape = av_guess_sample_aspect_ratio(decoder->format, stream, 0);
         if (pixel_shape.num > 0 && pixel_shape.den > 0) {
            width = width * (F64)pixel_shape.num / (F64)pixel_shape.den;
         }
         F64 longest = Max(width, height);
         if (max_dim > 0 && longest > (F64)max_dim) {
            width = width * (F64)max_dim / longest;
            height = height * (F64)max_dim / longest;
         }
         decoder->size = V2S32(Max((S32)(width + 0.5), 1), Max((S32)(height + 0.5), 1));

         //- bgz: time
         decoder->seconds_per_tick = av_q2d(stream->time_base);
         decoder->start_ticks = (stream->start_time != AV_NOPTS_VALUE) ? stream->start_time : 0;

         AVRational rate = av_guess_frame_rate(decoder->format, stream, 0);
         F64        frames_per_second = (rate.num > 0 && rate.den > 0) ? av_q2d(rate) : 0;
         decoder->frame_s = (frames_per_second > 0) ? 1.0 / frames_per_second : 0;

         F64 duration_s = 0;
         if (stream->duration != AV_NOPTS_VALUE && stream->duration > 0) {
            duration_s = (F64)stream->duration * decoder->seconds_per_tick;
         } else if (decoder->format->duration != AV_NOPTS_VALUE && decoder->format->duration > 0) {
            duration_s = (F64)decoder->format->duration / (F64)AV_TIME_BASE;
         }

         info_out->size = decoder->size;
         info_out->duration_s = duration_s;
         info_out->frames_per_second = frames_per_second;

         //- bgz: sound, if it is wanted and the file has some
         //
         // The stream that goes with the video. A file whose sound can't be
         // decoded still plays, without it.
         const AVCodec* sound_codec = 0;
         int sound_idx = (sound_rate > 0) ? av_find_best_stream(decoder->format, AVMEDIA_TYPE_AUDIO, -1, decoder->stream_idx,
                                                                 &sound_codec, 0)
                                          : -1;
         if (sound_idx >= 0 && sound_codec != 0) {
            AVStream* sound_stream = decoder->format->streams[sound_idx];
            decoder->sound_codec = avcodec_alloc_context3(sound_codec);
            decoder->sound_frame = av_frame_alloc();
            B32 sound_is_open = (decoder->sound_codec != 0 && decoder->sound_frame != 0 &&
                                 avcodec_parameters_to_context(decoder->sound_codec, sound_stream->codecpar) >= 0 &&
                                 avcodec_open2(decoder->sound_codec, sound_codec, 0) >= 0);
            if (sound_is_open) {
               decoder->sound_stream_idx = sound_idx;
               decoder->sound_rate = sound_rate;
               decoder->sound_seconds_per_tick = av_q2d(sound_stream->time_base);
               decoder->sound_zero_s = (F64)decoder->start_ticks * decoder->seconds_per_tick;
               info_out->has_sound = 1;

               // Sound that goes on past the last frame is played to its
               // end, so the video is as long as its sound.
               if (sound_stream->duration != AV_NOPTS_VALUE && sound_stream->duration > 0) {
                  S64 sound_start = (sound_stream->start_time != AV_NOPTS_VALUE) ? sound_stream->start_time : 0;
                  F64 sound_end_s = (F64)(sound_start + sound_stream->duration) * decoder->sound_seconds_per_tick -
                                    decoder->sound_zero_s;
                  info_out->duration_s = Max(info_out->duration_s, sound_end_s);
               }
            } else {
               LogUserErrorF("The sound of %S can't be decoded (%s): it plays without\n", path, sound_codec->name);
               av_frame_free(&decoder->sound_frame);
               avcodec_free_context(&decoder->sound_codec);
            }
         }
      }
   }

   ScratchEnd(scratch);

   if (!is_open) {
      VID_DecoderClose(decoder);
      decoder = 0;
   }
   return decoder;
}

internal B32
VID_DecoderMediaInfo(String8 path, VID_MediaInfo* info_out) {
   MemoryZeroStruct(info_out);
   B32  result = 0;
   Temp scratch = ScratchBegin(0, 0);
   av_log_set_level(AV_LOG_ERROR);

   AVFormatContext* format = 0;
   String8          path_z = PushStr8Copy(scratch.arena, path);
   if (avformat_open_input(&format, (char*)path_z.str, 0, 0) >= 0) {
      int stream_idx = (avformat_find_stream_info(format, 0) >= 0) ? av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, 0, 0)
                                                                   : -1;
      if (stream_idx >= 0) {
         AVStream* stream = format->streams[stream_idx];
         F64       width = (F64)stream->codecpar->width;
         F64       height = (F64)stream->codecpar->height;
         AVRational pixel_shape = av_guess_sample_aspect_ratio(format, stream, 0);
         if (pixel_shape.num > 0 && pixel_shape.den > 0) {
            width = width * (F64)pixel_shape.num / (F64)pixel_shape.den;
         }
         info_out->size = V2S32(Max((S32)(width + 0.5), 1), Max((S32)(height + 0.5), 1));

         AVRational rate = av_guess_frame_rate(format, stream, 0);
         if (rate.num > 0 && rate.den > 0) {
            info_out->rate_num = rate.num;
            info_out->rate_den = rate.den;
         }
         if (stream->duration != AV_NOPTS_VALUE && stream->duration > 0) {
            info_out->duration_s = (F64)stream->duration * av_q2d(stream->time_base);
         } else if (format->duration != AV_NOPTS_VALUE && format->duration > 0) {
            info_out->duration_s = (F64)format->duration / (F64)AV_TIME_BASE;
         }

         int sound_idx = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, stream_idx, 0, 0);
         if (sound_idx >= 0) {
            AVCodecParameters* sound = format->streams[sound_idx]->codecpar;
            info_out->has_sound = 1;
            info_out->sound_rate = sound->sample_rate;
            info_out->sound_channels = sound->ch_layout.nb_channels;
         }

         // The timecode: the file's own, the video's, or that of a stream
         // that is only there to hold it (a .mov's tmcd).
         AVDictionaryEntry* timecode = av_dict_get(format->metadata, "timecode", 0, 0);
         if (timecode == 0) {
            timecode = av_dict_get(stream->metadata, "timecode", 0, 0);
         }
         for (unsigned int i = 0; timecode == 0 && i < format->nb_streams; i += 1) {
            timecode = av_dict_get(format->streams[i]->metadata, "timecode", 0, 0);
         }
         if (timecode != 0) {
            // 01:00:00:00, or 01:00:00;00 when it is drop-frame.
            S32   parts[4] = {0};
            S32   count = 0;
            B32   drops = 0;
            B32   in_number = 0;
            char* c = timecode->value;
            for (; *c != 0 && count <= 4; c += 1) {
               if (*c >= '0' && *c <= '9') {
                  if (!in_number) {
                     in_number = 1;
                     count += 1;
                  }
                  if (count <= 4) parts[count - 1] = parts[count - 1] * 10 + (*c - '0');
               } else {
                  in_number = 0;
                  drops = drops || (*c == ';');
               }
            }
            if (count == 4) {
               info_out->has_timecode = 1;
               MemoryCopy(info_out->timecode, parts, sizeof(parts));
               info_out->timecode_drops = drops;
            }
         }
         result = 1;
      }
      avformat_close_input(&format);
   }
   ScratchEnd(scratch);
   return result;
}

internal VID_Decoder*
VID_DecoderOpen(Arena* arena, String8 path, S32 max_dim, S32 sound_rate, VID_DecoderInfo* info_out) {
   return _VID_DecoderOpen(arena, path, max_dim, sound_rate, info_out, 0);
}

internal VID_Decoder*
VID_DecoderOpenOnCard(Arena* arena, String8 path, S32 max_dim, S32 sound_rate, VID_DecoderInfo* info_out) {
   return _VID_DecoderOpen(arena, path, max_dim, sound_rate, info_out, 1);
}

// Decodes the next frame into decoder->frame, which the caller unrefs, and
// says when it is shown. 0 at the end, or when it can't go on.
internal B32
_VID_DecodeFrame(VID_Decoder* decoder, F64* time_s_out) {
   B32 result = 0;

   // The codec is asked for a frame. When it wants more of the file first,
   // it is fed the next packet of the video stream and asked again - several
   // packets can go in before a frame comes out, and several frames can come
   // out of one.
   for (;;) {
      int error = avcodec_receive_frame(decoder->codec, decoder->frame);
      if (error == 0) {
         decoder->frames_since_seek += 1;
         result = 1;
         break;
      }
      if (error == AVERROR_EOF && decoder->frames_since_seek == 0 && !decoder->seek_was_to_start) {
         // A seek that found nothing to decode: from the start instead.
         _VID_SeekToStart(decoder);
         continue;
      }
      if (error != AVERROR(EAGAIN)) {
         // AVERROR_EOF: the end. Anything else: it can't go on.
         if (error != AVERROR_EOF) {
            Temp scratch = ScratchBegin(0, 0);
            LogUserErrorF("%S stopped decoding: %S\n", decoder->path, _VID_FFmpegError(scratch.arena, error));
            ScratchEnd(scratch);
         }
         break;
      }
      if (decoder->is_draining) {
         break; // asked for more after being told there is none
      }

      if (_VID_NextPacket(decoder, 0)) {
         // A packet it can't take is passed over: one bad one does not end
         // the video.
         avcodec_send_packet(decoder->codec, decoder->packet);
         av_packet_unref(decoder->packet);
      } else {
         // No more of the file. A packet of nothing tells the codec to give
         // up the frames it is holding.
         decoder->is_draining = 1;
         avcodec_send_packet(decoder->codec, 0);
      }
   }

   if (result) {
      AVFrame* frame = decoder->frame;

      //- bgz: when it is shown
      S64 ticks = frame->best_effort_timestamp;
      F64 time_s = 0;
      if (ticks != AV_NOPTS_VALUE) {
         time_s = (F64)(ticks - decoder->start_ticks) * decoder->seconds_per_tick;
      } else {
         // The file does not say: a frame after the last.
         time_s = decoder->last_time_s + ((decoder->frame_s > 0) ? decoder->frame_s : 0.04);
      }
      time_s = Max(time_s, 0.0);
      decoder->last_time_s = time_s;
      *time_s_out = time_s;
   }

   return result;
}

// decoder->frame into pixels, as RGBA at the size given out, and lets go of
// it. 0 if it can't be.
internal B32
_VID_ConvertFrame(VID_Decoder* decoder, U8* pixels) {
   B32      result = 1;
   AVFrame* frame = decoder->frame;

   // A frame on the card is brought back to memory first, in the form the
   // card has it.
   if (frame->format == AV_PIX_FMT_D3D11 && decoder->card_frame != 0) {
      av_frame_unref(decoder->card_frame);
      if (av_hwframe_transfer_data(decoder->card_frame, frame, 0) >= 0) {
         frame = decoder->card_frame;
      } else {
         LogUserErrorF("%S: a frame could not be brought back from the graphics card\n", decoder->path);
         result = 0;
      }
   }

   if (result) {
      // The context is kept from frame to frame, and made again only if the
      // frames change size or form part way through, as some files' do.
      decoder->scale = sws_getCachedContext(decoder->scale, frame->width, frame->height, (enum AVPixelFormat)frame->format,
                                            decoder->size.x, decoder->size.y, AV_PIX_FMT_RGBA, SWS_BILINEAR, 0, 0, 0);
      if (decoder->scale == 0) {
         LogUserErrorF("%S has frames in a form that can't be shown\n", decoder->path);
         result = 0;
      } else {
         uint8_t* dst_planes[4] = {pixels, 0, 0, 0};
         int      dst_strides[4] = {decoder->size.x * 4, 0, 0, 0};
         sws_scale(decoder->scale, (const uint8_t* const*)frame->data, frame->linesize, 0, frame->height, dst_planes,
                   dst_strides);
      }
   }
   av_frame_unref(decoder->frame);
   return result;
}

internal B32
VID_DecoderNextFrame(VID_Decoder* decoder, U8* pixels, F64* time_s_out) {
   return _VID_DecodeFrame(decoder, time_s_out) && _VID_ConvertFrame(decoder, pixels);
}

internal B32
VID_DecoderNextFrameFrom(VID_Decoder* decoder, F64 from_s, U8* pixels, F64* time_s_out) {
   B32 result = 0;
   while (_VID_DecodeFrame(decoder, time_s_out)) {
      if (*time_s_out >= from_s) {
         result = _VID_ConvertFrame(decoder, pixels);
         break;
      }
      av_frame_unref(decoder->frame);
   }
   return result;
}

// A frame of the file's sound, just decoded, into sound_pending as two
// channels of F32 at the rate given out.
internal void
_VID_ConvertSound(VID_Decoder* decoder) {
   AVFrame* frame = decoder->sound_frame;
   int      channels = frame->ch_layout.nb_channels;

   //- bgz: the converter, for sound in this form
   if (decoder->sound_convert == 0 || decoder->sound_in_rate != frame->sample_rate ||
       decoder->sound_in_format != frame->format || decoder->sound_in_channels != channels) {
      swr_free(&decoder->sound_convert);

      AVChannelLayout out_layout = AV_CHANNEL_LAYOUT_STEREO;
      AVChannelLayout in_layout = {0};
      if (frame->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC || channels <= 0) {
         // The file does not say which channel is which: the usual ones
         // for that many.
         av_channel_layout_default(&in_layout, (channels > 0) ? channels : 2);
      } else {
         av_channel_layout_copy(&in_layout, &frame->ch_layout);
      }
      int error = swr_alloc_set_opts2(&decoder->sound_convert, &out_layout, AV_SAMPLE_FMT_FLT, decoder->sound_rate, &in_layout,
                                      (enum AVSampleFormat)frame->format, frame->sample_rate, 0, 0);
      if (error < 0 || swr_init(decoder->sound_convert) < 0) {
         swr_free(&decoder->sound_convert);
      }
      av_channel_layout_uninit(&in_layout);

      decoder->sound_in_rate = frame->sample_rate;
      decoder->sound_in_format = frame->format;
      decoder->sound_in_channels = channels;
   }

   decoder->sound_pending_first = 0;
   decoder->sound_pending_count = 0;
   if (decoder->sound_convert == 0) {
      return; // sound in a form that can't be converted: none
   }

   //- bgz: when it is heard
   //
   // The converter holds back a few samples of what it was given before:
   // what comes out now begins that much earlier than this frame does.
   F64 held_s = (F64)swr_get_delay(decoder->sound_convert, decoder->sound_rate) / (F64)decoder->sound_rate;
   S64 ticks = frame->best_effort_timestamp;
   F64 time_s = decoder->sound_next_s;
   if (ticks != AV_NOPTS_VALUE) {
      time_s = (F64)ticks * decoder->sound_seconds_per_tick - decoder->sound_zero_s - held_s;
   }

   //- bgz: convert
   int out_cap = swr_get_out_samples(decoder->sound_convert, frame->nb_samples);
   if (out_cap > 0) {
      if ((U64)out_cap > decoder->sound_pending_cap) {
         F32* grown = (F32*)av_realloc(decoder->sound_pending, (size_t)out_cap * 2 * sizeof(F32));
         if (grown != 0) {
            decoder->sound_pending = grown;
            decoder->sound_pending_cap = (U64)out_cap;
         }
      }
      if ((U64)out_cap <= decoder->sound_pending_cap) {
         uint8_t* out_planes[1] = {(uint8_t*)decoder->sound_pending};
         int      got = swr_convert(decoder->sound_convert, out_planes, out_cap, (const uint8_t**)frame->extended_data,
                                    frame->nb_samples);
         if (got > 0) {
            decoder->sound_pending_count = (U64)got;
            decoder->sound_pending_time_s = time_s;
            decoder->sound_next_s = time_s + (F64)got / (F64)decoder->sound_rate;
         }
      }
   }
}

internal U64
VID_DecoderNextSound(VID_Decoder* decoder, F32* samples, U64 frame_cap, F64* time_s_out) {
   if (decoder->sound_codec == 0 || frame_cap == 0) {
      return 0;
   }

   // As for the video: the codec is asked, and fed the sound's next packet
   // when it wants more first.
   while (decoder->sound_pending_count == 0) {
      int error = avcodec_receive_frame(decoder->sound_codec, decoder->sound_frame);
      if (error == 0) {
         _VID_ConvertSound(decoder);
         av_frame_unref(decoder->sound_frame);
         continue;
      }
      if (error != AVERROR(EAGAIN) || decoder->sound_is_draining) {
         return 0; // the end, or it can't go on
      }
      if (_VID_NextPacket(decoder, 1)) {
         avcodec_send_packet(decoder->sound_codec, decoder->packet);
         av_packet_unref(decoder->packet);
      } else {
         decoder->sound_is_draining = 1;
         avcodec_send_packet(decoder->sound_codec, 0);
      }
   }

   U64 count = Min(frame_cap, decoder->sound_pending_count);
   MemoryCopy(samples, decoder->sound_pending + decoder->sound_pending_first * 2, count * 2 * sizeof(F32));
   *time_s_out = decoder->sound_pending_time_s;
   decoder->sound_pending_first += count;
   decoder->sound_pending_count -= count;
   decoder->sound_pending_time_s += (F64)count / (F64)decoder->sound_rate;
   return count;
}

internal void
VID_DecoderSeek(VID_Decoder* decoder, F64 time_s) {
   decoder->last_time_s = time_s;

   S64 ticks = decoder->start_ticks;
   if (decoder->seconds_per_tick > 0) {
      ticks += (S64)(time_s / decoder->seconds_per_tick);
   }

   // To the last frame that can be decoded on its own at or before the
   // time: what is between it and the time is decoded and thrown away by
   // the player. Some files can't be sought in: from the start is all that
   // is left.
   int error = -1;
   if (time_s > 0) {
      error = av_seek_frame(decoder->format, decoder->stream_idx, ticks, AVSEEK_FLAG_BACKWARD);
   }
   if (error < 0) {
      _VID_SeekToStart(decoder);
   } else {
      _VID_AfterSeek(decoder);
      decoder->seek_was_to_start = 0;
      decoder->sound_next_s = time_s;
   }
}
