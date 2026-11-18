////////////////////////////////
//~ bgz: embed: getting sound ready for the model
//
// See embed.h. As CLAP's own preprocessing (preprocessor_config.json in its
// model's folder, and ClapFeatureExtractor in Hugging Face's transformers,
// which this follows step for step): the sound is made 10 seconds long -
// a shorter one repeated as many whole times as fit, then silence - and
// turned into a picture of itself, a log-mel spectrogram: for each
// hundredth of a second, how loud it is in each of 64 bands of pitch.
//
//    1024 samples at a time, 480 on from the last, under a Hann window
//    their spectrum (an FFT), each bin's power
//    the 513 bins added up into 64 bands, 50 Hz to 14 kHz, spaced as the
//       ear hears (Slaney's mel scale, each band scaled by its width)
//    in decibels: 10 * log10

#define _EMB_FFT_SIZE 1024
#define _EMB_FFT_BINS (_EMB_FFT_SIZE / 2 + 1)
#define _EMB_HOP      480

// Made once, by the first EMB_SoundFeatures. One thread only calls it (the
// indexer's reader).
global B32 _emb_sound_ready = 0;
global F64 _emb_window[_EMB_FFT_SIZE];
global F64 _emb_fft_cos[_EMB_FFT_SIZE / 2];
global F64 _emb_fft_sin[_EMB_FFT_SIZE / 2];
global U32 _emb_fft_swap[_EMB_FFT_SIZE]; // each index with its 10 bits backwards
global F64 _emb_mel[EMB_SOUND_MELS][_EMB_FFT_BINS];
global U32 _emb_mel_first[EMB_SOUND_MELS]; // the bins a band is not zero for
global U32 _emb_mel_opl[EMB_SOUND_MELS];

internal F64
_EMB_MelFromHertz(F64 hz) {
   return (hz >= 1000.0) ? 15.0 + log(hz / 1000.0) * (27.0 / log(6.4)) : 3.0 * hz / 200.0;
}

internal F64
_EMB_HertzFromMel(F64 mel) {
   return (mel >= 15.0) ? 1000.0 * exp((log(6.4) / 27.0) * (mel - 15.0)) : 200.0 * mel / 3.0;
}

internal void
_EMB_SoundInit(void) {
   F64 pi = 3.14159265358979323846;
   for (U64 i = 0; i < _EMB_FFT_SIZE; i += 1) {
      _emb_window[i] = 0.5 - 0.5 * cos(2.0 * pi * (F64)i / (F64)_EMB_FFT_SIZE);
      U32 r = 0;
      for (U32 bit = 0; bit < 10; bit += 1) {
         if (i & (1ull << bit)) r |= 1u << (9 - bit);
      }
      _emb_fft_swap[i] = r;
   }
   for (U64 i = 0; i < _EMB_FFT_SIZE / 2; i += 1) {
      _emb_fft_cos[i] = cos(2.0 * pi * (F64)i / (F64)_EMB_FFT_SIZE);
      _emb_fft_sin[i] = -sin(2.0 * pi * (F64)i / (F64)_EMB_FFT_SIZE);
   }

   // The bands: triangles, each from the middle of the one before to the
   // middle of the one after, the middles evenly spaced in mels.
   F64 mel_min = _EMB_MelFromHertz(50.0);
   F64 mel_max = _EMB_MelFromHertz(14000.0);
   F64 hz[EMB_SOUND_MELS + 2];
   for (U64 i = 0; i < EMB_SOUND_MELS + 2; i += 1) {
      hz[i] = _EMB_HertzFromMel(mel_min + (mel_max - mel_min) * (F64)i / (F64)(EMB_SOUND_MELS + 1));
   }
   for (U64 m = 0; m < EMB_SOUND_MELS; m += 1) {
      F64 scale = 2.0 / (hz[m + 2] - hz[m]);
      _emb_mel_first[m] = _EMB_FFT_BINS;
      _emb_mel_opl[m] = 0;
      for (U64 bin = 0; bin < _EMB_FFT_BINS; bin += 1) {
         F64 f = (F64)(EMB_SOUND_RATE / 2) * (F64)bin / (F64)(_EMB_FFT_BINS - 1);
         F64 up = (f - hz[m]) / (hz[m + 1] - hz[m]);
         F64 down = (hz[m + 2] - f) / (hz[m + 2] - hz[m + 1]);
         F64 w = Max(0.0, Min(up, down)) * scale;
         _emb_mel[m][bin] = w;
         if (w != 0) {
            _emb_mel_first[m] = Min(_emb_mel_first[m], (U32)bin);
            _emb_mel_opl[m] = (U32)bin + 1;
         }
      }
   }
   _emb_sound_ready = 1;
}

internal void
EMB_SoundFeatures(F32* samples, U64 count, F32* features_out) {
   if (!_emb_sound_ready) {
      _EMB_SoundInit();
   }
   Temp scratch = ScratchBegin(0, 0);
   count = Min(count, EMB_SOUND_SAMPLES);

   // Ten seconds of it: repeated whole, then silence, with half a window of
   // itself mirrored at each end so the first and last windows are centred
   // on the first and last samples.
   U64  half = _EMB_FFT_SIZE / 2;
   F64* wave = PushArray(scratch.arena, F64, EMB_SOUND_SAMPLES + _EMB_FFT_SIZE);
   F64* body = wave + half;
   if (count != 0) {
      U64 repeats = EMB_SOUND_SAMPLES / count;
      for (U64 i = 0; i < repeats * count; i += 1) body[i] = samples[i % count];
   }
   for (U64 i = 0; i < half; i += 1) {
      wave[i] = body[half - i];
      body[EMB_SOUND_SAMPLES + i] = body[EMB_SOUND_SAMPLES - 2 - i];
   }

   // A sound repeated a whole number of windows' steps on is the same there
   // again: a window that lies wholly in a later repeat has the numbers of
   // the one a repeat before it, and is not worked out twice.
   U64 repeat_frames = (count != 0 && count % _EMB_HOP == 0) ? count / _EMB_HOP : 0;
   U64 repeats_end = (count != 0) ? (EMB_SOUND_SAMPLES / count) * count : 0;

   F64 re[_EMB_FFT_SIZE];
   F64 im[_EMB_FFT_SIZE];
   F64 power[_EMB_FFT_BINS];
   for (U64 frame = 0; frame < EMB_SOUND_FRAMES; frame += 1) {
      if (repeat_frames != 0 && frame * _EMB_HOP >= count + half && frame * _EMB_HOP + half <= repeats_end) {
         MemoryCopy(features_out + frame * EMB_SOUND_MELS, features_out + (frame - repeat_frames) * EMB_SOUND_MELS,
                    EMB_SOUND_MELS * sizeof(F32));
         continue;
      }
      F64* in = wave + frame * _EMB_HOP;
      for (U64 i = 0; i < _EMB_FFT_SIZE; i += 1) {
         re[_emb_fft_swap[i]] = in[i] * _emb_window[i];
         im[i] = 0;
      }

      // The FFT, in place: pairs, then fours, then eights...
      for (U64 size = 2; size <= _EMB_FFT_SIZE; size *= 2) {
         U64 step = _EMB_FFT_SIZE / size;
         for (U64 start = 0; start < _EMB_FFT_SIZE; start += size) {
            for (U64 k = 0; k < size / 2; k += 1) {
               F64 c = _emb_fft_cos[k * step];
               F64 s = _emb_fft_sin[k * step];
               U64 a = start + k;
               U64 b = a + size / 2;
               F64 tr = re[b] * c - im[b] * s;
               F64 ti = re[b] * s + im[b] * c;
               re[b] = re[a] - tr;
               im[b] = im[a] - ti;
               re[a] += tr;
               im[a] += ti;
            }
         }
      }
      for (U64 bin = 0; bin < _EMB_FFT_BINS; bin += 1) {
         power[bin] = re[bin] * re[bin] + im[bin] * im[bin];
      }

      for (U64 m = 0; m < EMB_SOUND_MELS; m += 1) {
         F64 sum = 0;
         for (U64 bin = _emb_mel_first[m]; bin < _emb_mel_opl[m]; bin += 1) {
            sum += _emb_mel[m][bin] * power[bin];
         }
         features_out[frame * EMB_SOUND_MELS + m] = (F32)(10.0 * log10(Max(sum, 1e-10)));
      }
   }
   ScratchEnd(scratch);
}
