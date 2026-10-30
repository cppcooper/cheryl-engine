# Audio codec fixtures

`tones.flac` and `tones.mp3` encode one second of repository-generated PCM16 stereo
at 48 kHz: left 440 Hz, right 660 Hz, each at amplitude 0.25. The source sample
formula is `round(8192 * sin(2 * pi * frequency * frame / 48000))`. These fixtures
contain no third-party recorded media.

[generate-codecs.py](generate-codecs.py) writes that owned waveform and encodes
FLAC s16 plus MP3 at 128 kbps with Xing/gapless metadata. It requires FFmpeg with
the FLAC and libmp3lame encoders. Regeneration prepares assets; it does not build
Cheryl, run a decoder, or establish engine acceptance. Encoder changes can change
the compressed bytes; retain the waveform/channel/rate contract when replacing
the fixtures.

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  python3 projects/modules/audio/miniaudio/tests/fixtures/generate-codecs.py
)
```

The codec regression verifies format, finite PCM, source duration within lossy
padding bounds, channel-specific frequencies and levels. FLAC additionally has
exact frame count and lossless sample checks. Short fixture decode does not
establish sustained streaming, native routing or perceptual codec quality.
