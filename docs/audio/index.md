# Audio

Prepare clips or streams, retain voices that need control, and close output after
its producers settle. Audio uses its own playback clock and application-owned
lifetime. The [Audio system](../engine-systems/audio.md) introduces the types.

```{toctree}
:maxdepth: 1

Control playback and ownership <../runtime/audio>
```

The [miniaudio module](../modules/miniaudio.md#preparation-and-output) supplies the
concrete setup example and independent device/offline procedures.
