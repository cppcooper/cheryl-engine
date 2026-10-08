# Audio ownership and playback

The public [audio contracts](../../projects/engine/include/cheryl/audio.h) belong
to Engine and contain no SDK, device or graphics types. Applications own an audio
system independently of `EngineContext` and compose a selected audio module through
its public CMake target. Engine has no reverse dependency on that module.

## Clips and voices

`Audio::Clip::from_samples` takes owned interleaved float PCM, with mono/stereo
channels and a sample rate between 8 and 192 kHz. Data is nonempty, frames are
complete, and every sample is finite. The nominal sample range is `[-1, 1]`;
storage does not clamp samples or change their rate. Copies share immutable
storage. Moving preserves the original valid snapshot. Clip creation does not
open a device. Its frame count and duration describe source audio, independently
of a mixer's output rate.

`Audio::System::play` creates a voice with an independent cursor. Voices can share
a clip without sharing playback progress. `stream` opens file-backed playback for
music; the file must remain available until playback ends. Decode and initial file
opening are synchronous preparation operations, so applications perform them at
loading boundaries or dispatch owned decode requests through their worker groups.

Voice and master volumes are normalized linear gains in `[0, 1]`, with zero as
mute. Invalid/non-finite gains throw. Multiple voices are mixed; overlapping loud
sources can exceed nominal PCM amplitude and applications manage their mix levels.

| Operation/state | Behavior |
| --- | --- |
| Initial playback | Starts immediately, or starts `Paused` when requested. |
| `pause` | Pauses a playing voice and preserves progress. |
| `resume` | Resumes a paused voice; other live states are unchanged. |
| `stop` | Leaves the voice `Stopped` until explicit restart. |
| Natural end | Reports `Finished`; explicit restart is required. |
| `restart` | Requests source frame zero and starts playback. |
| `set_looping` | Changes the loop policy without restarting stopped/finished playback. |
| System closure | Surviving handles report `Closed`; their controls throw. |

Snapshots copy playback state, volume, loop policy and known source duration.
They are observations, not an atomic freeze of native playback. Live decoder
cursors are outside the initial API; reading them concurrently with mixing needs
additional synchronization. No game callback runs on the mixing thread.

## Lifetime and scheduling

Controls, submissions, snapshots and maintenance are synchronized across
application producers. Their order follows control admission, rather than a
deterministic simulation clock. Device playback progresses on its own clock
without render or simulation admission. Offline processing is explicitly selected;
an unavailable native device must not silently produce inaudible success.

The system retains active playback after the application releases its voice
handle. Call `maintain` regularly to reclaim completed/stopped/paused voices that
have no application handles. Maintenance does not advance playback. Discarded
looping voices remain active until system closure; retain their handles when they
need individual control.

Create the system before playback. Settle all producers before destroying it.
`close` is idempotent, stops output, retires native voices before their mixer/device,
and leaves surviving handles safe. A system cannot reopen; construct another
system for a new session. Audio need not share the graphics owner's thread, and
this contract does not add audio to the runtime's existing adapter teardown.

Audible native output and sustained streaming remain in the
[integration plan](../planning/audio-integration.md). The
[miniaudio module](../../projects/modules/audio/miniaudio/README.md) supplies CPU
decode, explicit device/offline output and streamed playback. Its independent
consumer provides native observation without a game, window or graphics module.

World, entity and collision organization belongs
to the game or its application-selected modules. FMOD is a later optional backend;
Studio authoring formats/capabilities require a separate contract.
