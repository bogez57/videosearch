////////////////////////////////
//~ bgz: picks
//
// See picks.h.

// The file: a pick a line, its start, end and best moment in thousandths of
// a second, its video's path and what was searched for, with tabs between.
internal void
_PCK_Save(PCK_Picks* picks) {
   Temp        scratch = ScratchBegin(0, 0);
   String8List lines = {0};
   for (U64 i = 0; i < picks->count; i += 1) {
      PCK_Pick* pick = &picks->v[i];
      Str8ListPushF(scratch.arena, &lines, "%llu\t%llu\t%llu\t%S\t%S\n", (unsigned long long)(pick->start_s * 1000.0 + 0.5),
                    (unsigned long long)(pick->end_s * 1000.0 + 0.5), (unsigned long long)(pick->best_s * 1000.0 + 0.5),
                    pick->path, pick->query);
   }
   // Not being able to write it loses only the list.
   WriteDataToFilePath(picks->file_path, Str8ListJoin(scratch.arena, &lines, 0));
   ScratchEnd(scratch);
}

internal PCK_Picks*
PCK_Alloc(String8 file_path) {
   Arena*     arena = ArenaAlloc();
   PCK_Picks* picks = PushArray(arena, PCK_Picks, 1);
   picks->arena = ArenaAlloc();
   picks->file_path = PushStr8Copy(arena, file_path);

   Temp    scratch = ScratchBegin(0, 0);
   String8 data = DataFromFilePath(scratch.arena, file_path);
   U64     pos = 0;
   while (pos < data.size && picks->count < PCK_CAP) {
      U64 line_start = pos;
      while (pos < data.size && data.str[pos] != '\n') pos += 1;
      String8 line = Str8Substr(data, R1U64(line_start, pos));
      pos += 1;
      if (line.size != 0 && line.str[line.size - 1] == '\r') line.size -= 1;

      // Five fields; the last, the query, is what is left.
      String8 fields[5] = {0};
      U64     field_count = 0;
      U64     at = 0;
      while (field_count < 4) {
         U64 tab = Str8FindNeedle(line, at, Str8Lit("\t"), 0);
         if (tab >= line.size) break;
         fields[field_count++] = Str8Substr(line, R1U64(at, tab));
         at = tab + 1;
      }
      if (field_count == 4) {
         fields[4] = Str8Substr(line, R1U64(at, line.size));
         F64 seconds[3] = {0};
         for (U64 f = 0; f < 3; f += 1) {
            U64 ms = 0;
            for (U64 i = 0; i < fields[f].size && fields[f].str[i] >= '0' && fields[f].str[i] <= '9'; i += 1) {
               ms = ms * 10 + (U64)(fields[f].str[i] - '0');
            }
            seconds[f] = (F64)ms / 1000.0;
         }
         PCK_Pick* pick = &picks->v[picks->count++];
         pick->start_s = seconds[0];
         pick->end_s = seconds[1];
         pick->best_s = seconds[2];
         pick->path = PushStr8Copy(picks->arena, fields[3]);
         pick->query = PushStr8Copy(picks->arena, fields[4]);
      }
   }
   ScratchEnd(scratch);
   return picks;
}

internal S64
PCK_Find(PCK_Picks* picks, String8 path, F64 start_s, F64 end_s) {
   S64 result = -1;
   for (U64 i = 0; i < picks->count; i += 1) {
      PCK_Pick* pick = &picks->v[i];
      if (fabs(pick->start_s - start_s) < 0.001 && fabs(pick->end_s - end_s) < 0.001 &&
          Str8Match(pick->path, path, StringMatchFlag_CaseInsensitive)) {
         result = (S64)i;
         break;
      }
   }
   return result;
}

internal B32
PCK_Add(PCK_Picks* picks, String8 path, String8 query, F64 start_s, F64 end_s, F64 best_s) {
   B32 result = (picks->count < PCK_CAP && PCK_Find(picks, path, start_s, end_s) < 0);
   if (result) {
      PCK_Pick* pick = &picks->v[picks->count++];
      pick->path = PushStr8Copy(picks->arena, path);
      pick->query = PushStr8Copy(picks->arena, query);
      pick->start_s = start_s;
      pick->end_s = end_s;
      pick->best_s = best_s;
      _PCK_Save(picks);
   }
   return result;
}

internal void
PCK_Remove(PCK_Picks* picks, U64 idx) {
   if (idx < picks->count) {
      MemoryCopy(&picks->v[idx], &picks->v[idx + 1], (picks->count - idx - 1) * sizeof(PCK_Pick));
      picks->count -= 1;
      _PCK_Save(picks);
   }
}

internal void
PCK_Clear(PCK_Picks* picks) {
   picks->count = 0;
   ArenaClear(picks->arena);
   _PCK_Save(picks);
}

internal void
PCK_Move(PCK_Picks* picks, U64 idx, S64 by) {
   S64 to = (S64)idx + by;
   if (idx < picks->count && to >= 0 && to < (S64)picks->count) {
      PCK_Pick moved = picks->v[idx];
      picks->v[idx] = picks->v[to];
      picks->v[to] = moved;
      _PCK_Save(picks);
   }
}

internal void
PCK_SetRange(PCK_Picks* picks, U64 idx, F64 start_s, F64 end_s) {
   if (idx < picks->count) {
      picks->v[idx].start_s = start_s;
      picks->v[idx].end_s = end_s;
      _PCK_Save(picks);
   }
}

internal F64
PCK_Seconds(PCK_Picks* picks) {
   F64 result = 0;
   for (U64 i = 0; i < picks->count; i += 1) {
      result += Max(picks->v[i].end_s - picks->v[i].start_s, 0.0);
   }
   return result;
}

////////////////////////////////
//~ bgz: the timeline
//
// FCPXML, version 1.9: the videos the clips are cut from (asset), the frame
// rates and sizes they have (format), and one sequence whose spine is the
// clips, one after another. Every time in it is a whole number of frames,
// written as a fraction of seconds: 1001/24000s is one frame at 23.976 a
// second.

// A video the picks are cut from, as the file says it is.
typedef struct _PCK_Media _PCK_Media;
struct _PCK_Media {
   String8 path;
   B32     ok;
   U64     format;       // which of the formats, from 0
   U64     rate_num;     // frames a second, as one of the rates editing programs have
   U64     rate_den;
   U64     frames;       // how long it is
   U64     first_frame;  // its timecode's, counted from 00:00:00:00
   VID_MediaInfo info;
};

typedef struct _PCK_Format _PCK_Format;
struct _PCK_Format {
   U64     rate_num;
   U64     rate_den;
   Vec2S32 size;
};

// frames at rate_num over rate_den a second, as seconds: "1001/24000s".
internal String8
_PCK_Time(Arena* arena, U64 frames, U64 rate_num, U64 rate_den) {
   U64 num = frames * rate_den;
   return (num % rate_num == 0) ? PushStr8F(arena, "%llus", (unsigned long long)(num / rate_num))
                                : PushStr8F(arena, "%llu/%llus", (unsigned long long)num, (unsigned long long)rate_num);
}

// text with what XML keeps for itself written its other way, for between
// the quotes of an attribute.
internal String8
_PCK_ForXML(Arena* arena, String8 text) {
   Temp        scratch = ScratchBegin(&arena, 1);
   String8List parts = {0};
   U64         from = 0;
   for (U64 i = 0; i <= text.size; i += 1) {
      char* other = 0;
      if (i < text.size) {
         U8 c = text.str[i];
         other = (c == '&') ? "&amp;" : (c == '<') ? "&lt;" : (c == '>') ? "&gt;" : (c == '"') ? "&quot;" : (c < 32) ? " " : 0;
      }
      if (i == text.size || other != 0) {
         Str8ListPush(scratch.arena, &parts, Str8Substr(text, R1U64(from, i)));
         if (other != 0) Str8ListPush(scratch.arena, &parts, Str8CString(other));
         from = i + 1;
      }
   }
   String8 result = Str8ListJoin(arena, &parts, 0);
   ScratchEnd(scratch);
   return result;
}

// A path as a file: URL: D:\a b\c.mp4 is file:///D:/a%20b/c.mp4, and
// \\nas\videos\c.mp4 is file://nas/videos/c.mp4.
internal String8
_PCK_URL(Arena* arena, String8 path) {
   Temp        scratch = ScratchBegin(&arena, 1);
   String8List parts = {0};
   B32         on_network = (path.size >= 2 && path.str[0] == '\\' && path.str[1] == '\\');
   Str8ListPush(scratch.arena, &parts, on_network ? Str8Lit("file://") : Str8Lit("file:///"));
   for (U64 i = on_network ? 2 : 0; i < path.size; i += 1) {
      U8 c = path.str[i];
      if (c == '\\') {
         Str8ListPush(scratch.arena, &parts, Str8Lit("/"));
      } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' || c == ':' ||
                 c == '-' || c == '_' || c == '.' || c == '~') {
         Str8ListPush(scratch.arena, &parts, Str8Substr(path, R1U64(i, i + 1)));
      } else {
         Str8ListPushF(scratch.arena, &parts, "%%%02X", (unsigned int)c);
      }
   }
   String8 result = Str8ListJoin(arena, &parts, 0);
   ScratchEnd(scratch);
   return result;
}

internal PCK_Exported
PCK_Export(PCK_Picks* picks, String8 file_path) {
   PCK_Exported result = {0};
   Temp         scratch = ScratchBegin(0, 0);
   Arena*       arena = scratch.arena;
   String8      name = Str8SkipLastSlash(file_path);
   String8      extension = Str8SkipLastDot(name);
   if (extension.size < name.size) {
      name = Str8Prefix(name, name.size - extension.size - 1);
   }

   //- bgz: the videos, each once, and the formats they have between them
   _PCK_Media*  media = PushArray(arena, _PCK_Media, picks->count + 1);
   U64          media_count = 0;
   _PCK_Format* formats = PushArray(arena, _PCK_Format, picks->count + 1);
   U64          format_count = 0;
   U64*         media_of = PushArray(arena, U64, picks->count + 1);
   for (U64 i = 0; i < picks->count; i += 1) {
      String8 path = picks->v[i].path;
      U64     m = 0;
      while (m < media_count && !Str8Match(media[m].path, path, StringMatchFlag_CaseInsensitive)) m += 1;
      media_of[i] = m;
      if (m < media_count) {
         continue;
      }
      _PCK_Media* one = &media[media_count++];
      one->path = path;
      one->ok = (VID_DecoderMediaInfo(path, &one->info) && one->info.rate_num > 0 && one->info.rate_den > 0);
      if (!one->ok) {
         LogUserErrorF("%S could not be read, so its picks are not in the timeline\n", path);
         continue;
      }

      // Its frame rate as the nearest of the ones editing programs have,
      // if it is within a two-hundredth of one: a file's own can be a
      // little off (23.98 as 2997 over 125).
      local_persist U64 rates[][2] = {{24000, 1001}, {24, 1}, {25, 1}, {30000, 1001}, {30, 1}, {48, 1},
                                                {50, 1},       {60000, 1001}, {60, 1}, {120000, 1001}, {120, 1}};
      F64 rate = (F64)one->info.rate_num / (F64)one->info.rate_den;
      one->rate_num = (U64)(rate * 1000.0 + 0.5);
      one->rate_den = 1000;
      F64 nearest = 0.005;
      for (U64 r = 0; r < ArrayCount(rates); r += 1) {
         F64 off = fabs(rate / ((F64)rates[r][0] / (F64)rates[r][1]) - 1.0);
         if (off < nearest) {
            nearest = off;
            one->rate_num = rates[r][0];
            one->rate_den = rates[r][1];
         }
      }
      rate = (F64)one->rate_num / (F64)one->rate_den;
      one->frames = (U64)(one->info.duration_s * rate + 0.5);

      // Its first frame's timecode as a count of frames: at a whole number
      // of them a second, less the numbers drop-frame timecode skips (two
      // a minute at 29.97, but every tenth minute).
      if (one->info.has_timecode) {
         U64 whole_rate = (U64)(rate + 0.5);
         U64 minutes = (U64)one->info.timecode[0] * 60 + (U64)one->info.timecode[1];
         one->first_frame = (minutes * 60 + (U64)one->info.timecode[2]) * whole_rate + (U64)one->info.timecode[3];
         if (one->info.timecode_drops) {
            U64 skipped = (whole_rate / 30) * 2 * (minutes - minutes / 10);
            one->first_frame -= Min(one->first_frame, skipped);
         }
      }

      U64 f = 0;
      while (f < format_count && !(formats[f].rate_num == one->rate_num && formats[f].rate_den == one->rate_den &&
                                   formats[f].size.x == one->info.size.x && formats[f].size.y == one->info.size.y)) {
         f += 1;
      }
      if (f == format_count) {
         formats[format_count].rate_num = one->rate_num;
         formats[format_count].rate_den = one->rate_den;
         formats[format_count].size = one->info.size;
         format_count += 1;
      }
      one->format = f;
   }

   //- bgz: the clips. The timeline has the frame rate and size of the first
   // clip's video. A clip is as long on it as it is in its video, to the
   // timeline's nearest frame.
   String8List clips = {0};
   U64         timeline_num = 0;
   U64         timeline_den = 0;
   U64         timeline_format = 0;
   B32         timeline_drops = 0;
   U64         timeline_frames = 0;
   for (U64 i = 0; i < picks->count; i += 1) {
      PCK_Pick*   pick = &picks->v[i];
      _PCK_Media* one = &media[media_of[i]];
      if (!one->ok) {
         result.left_out += 1;
         continue;
      }
      F64 rate = (F64)one->rate_num / (F64)one->rate_den;
      B32 whole = !(pick->end_s > pick->start_s);
      U64 first = whole ? 0 : (U64)(pick->start_s * rate + 0.5);
      U64 opl = whole ? one->frames : (U64)(pick->end_s * rate + 0.5);
      if (one->frames != 0) {
         opl = Min(opl, one->frames);
      }
      if (opl <= first) {
         result.left_out += 1;
         continue;
      }
      if (timeline_num == 0) {
         timeline_num = one->rate_num;
         timeline_den = one->rate_den;
         timeline_format = one->format;
         timeline_drops = one->info.timecode_drops;
      }
      F64 seconds = (F64)(opl - first) / rate;
      U64 frames = Max((U64)(seconds * (F64)timeline_num / (F64)timeline_den + 0.5), 1);

      String8 clip_name = _PCK_ForXML(arena, Str8SkipLastSlash(one->path));
      Str8ListPushF(arena, &clips,
                    "                        <asset-clip ref=\"r%llu\" offset=\"%S\" name=\"%S\" start=\"%S\" duration=\"%S\" "
                    "format=\"r%llu\" tcFormat=\"%s\"/>\n",
                    (unsigned long long)(format_count + media_of[i] + 1),
                    _PCK_Time(arena, timeline_frames, timeline_num, timeline_den), clip_name,
                    _PCK_Time(arena, one->first_frame + first, one->rate_num, one->rate_den),
                    _PCK_Time(arena, frames, timeline_num, timeline_den), (unsigned long long)(one->format + 1),
                    one->info.timecode_drops ? "DF" : "NDF");
      timeline_frames += frames;
      result.clips += 1;
   }

   //- bgz: the file
   if (result.clips != 0) {
      String8List out = {0};
      Str8ListPushF(arena, &out, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE fcpxml>\n<fcpxml version=\"1.9\">\n    <resources>\n");
      for (U64 f = 0; f < format_count; f += 1) {
         Str8ListPushF(arena, &out, "        <format id=\"r%llu\" frameDuration=\"%S\" width=\"%d\" height=\"%d\"/>\n",
                       (unsigned long long)(f + 1), _PCK_Time(arena, 1, formats[f].rate_num, formats[f].rate_den),
                       formats[f].size.x, formats[f].size.y);
      }
      for (U64 m = 0; m < media_count; m += 1) {
         _PCK_Media* one = &media[m];
         if (!one->ok) {
            continue;
         }
         String8 sound = {0};
         if (one->info.has_sound) {
            sound = PushStr8F(arena, " hasAudio=\"1\" audioSources=\"1\" audioChannels=\"%d\" audioRate=\"%d\"",
                              Max(one->info.sound_channels, 1), one->info.sound_rate);
         }
         Str8ListPushF(arena, &out,
                       "        <asset id=\"r%llu\" name=\"%S\" start=\"%S\" duration=\"%S\" hasVideo=\"1\" format=\"r%llu\"%S>\n"
                       "            <media-rep kind=\"original-media\" src=\"%S\"/>\n        </asset>\n",
                       (unsigned long long)(format_count + m + 1), _PCK_ForXML(arena, Str8SkipLastSlash(one->path)),
                       _PCK_Time(arena, one->first_frame, one->rate_num, one->rate_den),
                       _PCK_Time(arena, one->frames, one->rate_num, one->rate_den), (unsigned long long)(one->format + 1),
                       sound, _PCK_URL(arena, one->path));
      }
      String8 name_xml = _PCK_ForXML(arena, name);
      Str8ListPushF(arena, &out,
                    "    </resources>\n    <library>\n        <event name=\"%S\">\n            <project name=\"%S\">\n"
                    "                <sequence format=\"r%llu\" duration=\"%S\" tcStart=\"0s\" tcFormat=\"%s\" "
                    "audioLayout=\"stereo\" audioRate=\"48k\">\n                    <spine>\n",
                    name_xml, name_xml, (unsigned long long)(timeline_format + 1),
                    _PCK_Time(arena, timeline_frames, timeline_num, timeline_den), timeline_drops ? "DF" : "NDF");
      for (String8Node* n = clips.first; n != 0; n = n->next) Str8ListPush(arena, &out, n->string);
      Str8ListPushF(arena, &out, "                    </spine>\n                </sequence>\n            </project>\n"
                                 "        </event>\n    </library>\n</fcpxml>\n");
      result.written = WriteDataToFilePath(file_path, Str8ListJoin(arena, &out, 0));
   }

   LogInfoF("Timeline %S: %llu clips, %llu left out, %s\n", file_path, (unsigned long long)result.clips,
            (unsigned long long)result.left_out, result.written ? "written" : "not written");
   ScratchEnd(scratch);
   return result;
}
