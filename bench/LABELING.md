# Labeling the frames

`labels.tsv`, in the bench folder: one line for every frame,

    v<video> TAB <second> TAB <tags> TAB <caption>

- Look at each contact sheet (`sheets/v<video>_<first second>.jpg`, 20 frames; the yellow text over
  a frame is its video and second). When a frame is too small to be sure of at the sheet's size,
  open the frame itself (`frames/v<video>/<second, five digits>.jpg`).
- Label what can be seen in the frame, not what is known of the film.
- tags: every tag from `tags.txt` that is clearly true of the frame, comma separated. A tag with `?`
  after it when it is only partly true or can't be told for sure (a tiny figure that might be a
  soldier: `soldier?`). Something notable that no tag covers: a tag of your own (lower case,
  underscores).
- caption: one plain sentence of up to about 20 words: who or what, doing what, where, how it is
  shot. Words on the screen as they are written.
- Every frame gets a line, a black one too (`black_frame`).

What the tags mean, where it is not plain:

- `person`: any human that can be seen; with it `man`, `woman` or `child` when that can be told.
  `two_people`: exactly two people, both prominent. `crowd`: about eight people or more.
  `face_closeup`: one face fills most of the frame's height. `soldier`: in military uniform or helmet.
- `screaming`: mouth wide open: shouting, yelling or screaming. `singing`: mouth open at a
  microphone or plainly singing. `looking_up`: looking up at something above.
- `night`, `daylight`: outdoors and clearly one or the other. `interior`: inside a building,
  vehicle, cave, tunnel or dugout.
- `fire`: flames can be seen. `explosion`: a blast, a fireball, or earth and debris thrown up.
  `smoke`, `dust`: plainly there.
- `water`: any body of water; `river` or `sea` with it when that can be told.
- `title_text`: a title card, or large words put over the picture. `lyrics_text`: subtitles or a
  song's words over the picture. `logo`: a studio's or company's logo. `credits`: small-print
  credits. `black_frame`: all or nearly all black.
- `wide_shot`: a wide view; people in it, if any, are small. `closeup`: a head and shoulders or
  closer, or one thing filling the frame. `aerial_view`: seen from high above.
