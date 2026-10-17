# Create a HUD

Create an application-owned score display that retains the last complete text
generation while its replacement uploads. This extends the Player/Enemy game
with screen information and safe asynchronous resource replacement. Interactive
widgets are covered separately in [Create an interactive GUI](../how-to/create-gui.md).

## Responsibilities and state

HUD owns font-byte snapshots, its material handle, requested/displayed scores,
one pending upload and the current immutable text generation. Game logic owns
the score's meaning. HUD receives score changes and publishes drawable state;
it does not reach into Player or own the runtime.

Create `hud.h`:

```{literalinclude} ../../../projects/apps/tutorial-game/src/hud.h
:language: cpp
```

Construction runs on the graphics/platform owner because it uploads the initial
generation. Later `set_score()`/`update()` run on simulation. The dispatched
callback captures prepared data rather than `this`, so accepted work does not
borrow HUD. Font snapshots are retained by layout/prepared values.

## Construct during initialization

Replace First Game's `score_` generation member with
`std::optional<Tutorial::Hud> hud_`, add `int points_ = 0`, and retain a separate
`CE::Rendering::Camera2D hud_camera_`. Include `hud.h`. Keep its existing font
selection/loading and validated `text_` material, then replace initial score
layout/upload with:

```cpp
hud_.emplace(engine_, std::move(fonts), text_);
hud_camera_.set_framebuffer_size(engine_.window().framebuffer_size());
```

Keep the existing camera/input/material initialization. If construction fails,
the optional remains empty and ordinary partial-safe game cleanup applies.

## Update content on simulation

After quit handling and the Player/Enemy updates:

```cpp
hud_camera_.set_framebuffer_size(tick.framebuffer_size);
if (tick.input.button(next_cell_).pressed())
    hud_->set_score(++points_);
hud_->update();
```

Here `next_cell_` is First Game's existing Space action, repurposed as the
tutorial's score increment. Real game rules decide when to award points. Use an
appropriate bounded score type/policy if the application can approach integer
limits.

HUD keeps at most one upload pending and coalesces later changes into the next
request. This example performs small-label CPU preparation on simulation; larger
content can move that stage to a tracked worker group. Ready upload failures
propagate to the runtime's normal failure handling rather than disappearing in
an unread future.

## Publish a screen-space pass

After the world pass containing Player/Enemy:

```cpp
auto hud_pass = frame.begin_pass(hud_camera_.projection_matrix(), hud_camera_.view_matrix());
hud_->append(hud_pass, {24.0f, static_cast<float>(hud_camera_.framebuffer_size().height) - 48.0f});
```

The HUD camera keeps its identity view, so world scrolling does not move the
score. It still follows framebuffer resize observations. Its text packets retain
the chosen generation; a later replacement cannot alter an already published
frame. Remove the old direct score draw block to avoid drawing both versions.

## Release and observe

Replace `score_.reset()` with `hud_.reset()` in `deinit()`, before dropping the
game's materials. Runtime settles accepted context work and recycles frames
before that hook. HUD's saved endpoint retains no borrowed context pointer and
its upload captures retain only owned preparation data.

The score should start at zero and change after Space presses, remain visible
while an upload waits, and stay in screen coordinates as the world view changes.
Repeated unchanged updates should not submit replacement uploads. Run both
runtime modes when checking a game's integration; visual smoothness alone does
not establish correct teardown.

Read [text updates](../how-to/update-text.md),
[dispatch ownership](../../engine-systems/dispatch.md),
[text contracts](../../assets/text-layout.md) and
[shutdown ordering](../../runtime/runtime-frame-boundary.md#shutdown).
