# Test videos

Small videos made with FFmpeg's built-in test sources, for checking the
program without anyone's own videos: `bin\videosearch.exe data\test`.

- `testsrc_12s.mp4`: 12 s at 30 fps, 640x360. Should give 12 frames.
- `mandelbrot_7s.mp4`: 7 s at 25 fps, 320x240. Should give 7 frames.
- `broken.mp4`: not a video. Should count as one that could not be decoded.

Made with:

```
ffmpeg -f lavfi -i "testsrc2=size=640x360:rate=30:duration=12" -c:v libx264 -pix_fmt yuv420p -crf 32 testsrc_12s.mp4
ffmpeg -f lavfi -i "mandelbrot=size=320x240:rate=25" -t 7 -c:v libx264 -pix_fmt yuv420p -crf 32 mandelbrot_7s.mp4
```
