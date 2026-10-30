"""Create owned one-second codec fixtures; never invokes Cheryl or its tests."""

from pathlib import Path
import math
import struct
import subprocess
import tempfile
import wave


def main():
    destination = Path(__file__).resolve().parent
    with tempfile.TemporaryDirectory(prefix="cheryl-audio-codecs-") as temporary:
        source = Path(temporary) / "tones.wav"
        pcm = bytearray()
        for frame in range(48000):
            for frequency in (440, 660):
                pcm.extend(struct.pack("<h", round(8192 * math.sin(2 * math.pi * frequency * frame / 48000))))
        with wave.open(str(source), "wb") as output:
            output.setnchannels(2)
            output.setsampwidth(2)
            output.setframerate(48000)
            output.writeframes(pcm)
        for name, codec in (
            ("tones.flac", ("-c:a", "flac", "-sample_fmt", "s16")),
            ("tones.mp3", ("-c:a", "libmp3lame", "-b:a", "128k", "-write_xing", "1")),
        ):
            subprocess.run(
                ("ffmpeg", "-hide_banner", "-loglevel", "error", "-nostdin", "-y",
                 "-i", str(source), "-map_metadata", "-1", *codec, "-threads", "1", str(destination / name)),
                check=True,
            )


if __name__ == "__main__":
    main()
