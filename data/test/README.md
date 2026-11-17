# Test videos

Small videos made with FFmpeg's built-in test sources, for checking the
program without anyone's own videos: `bin\videosearch.exe data\test`.

- `testsrc_12s.mp4`: 12 s at 30 fps, 640x360. Should give 12 frames.
- `mandelbrot_7s.mp4`: 7 s at 25 fps, 320x240. Should give 7 frames.
- `sound_tone_silence_noise_15s.mp4`: a grey picture with 5 s of a steady
  tone, 5 s of silence and 5 s of hiss. Should give 15 frames and 2
  stretches of sound (the silence is left out).
- `broken.mp4`: not a video. Should count as one that could not be decoded.

Made with:

```
ffmpeg -f lavfi -i "testsrc2=size=640x360:rate=30:duration=12" -c:v libx264 -pix_fmt yuv420p -crf 32 testsrc_12s.mp4
ffmpeg -f lavfi -i "mandelbrot=size=320x240:rate=25" -t 7 -c:v libx264 -pix_fmt yuv420p -crf 32 mandelbrot_7s.mp4
ffmpeg -f lavfi -i "color=c=gray:s=320x180:r=15:d=15" -f lavfi -i "sine=frequency=880:sample_rate=48000:duration=5" -f lavfi -i "anullsrc=r=48000:cl=mono:d=5" -f lavfi -i "anoisesrc=color=white:sample_rate=48000:duration=5:amplitude=0.3" -filter_complex "[1][2][3]concat=n=3:v=0:a=1[a]" -map 0:v -map "[a]" -c:v libx264 -pix_fmt yuv420p -crf 35 -c:a aac -b:a 64k -shortest sound_tone_silence_noise_15s.mp4
```
