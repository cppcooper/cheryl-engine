# Display and update text

Extend [First Game's text walkthrough](../first-game.md#put-text-on-screen)
with a score that changes during play. Text crosses three stages: CPU
layout/preparation, graphics upload, then CPU frame submission. This task uses
the existing `first-game:text` material and keeps an earlier frame's resources
valid while the next label uploads.

## Retain the replacement state

Keep First Game's `score_` generation and `text_` material. Add these members to
the game:

```cpp
std::optional<CE::Text::FontCollection> fonts_;
CE::Runtime::PlatformDispatcher::Submission text_platform_;
std::future<CE::Text::Rendering::Generation> pending_score_;
int requested_score_ = 0;
int displayed_score_ = 0;
int submitted_score_ = 0;
```

Include `<cheryl/text/font-collection.h>`, `<cheryl/text/layout.h>`,
`<chrono>`, `<format>`, `<future>`, `<limits>`, `<optional>` and `<utility>`
alongside First Game's text/submission headers. The optional font collection
allows loading during `init()` and cleanup after partial initialization.

Game rules own the score's meaning. These three integers distinguish the latest
requested value, the value currently shown, and the value in the pending upload.

## Load fonts and an initial generation

Replace First Game's initial score block with this platform-owned `init()` code:

```cpp
text_platform_ = engine_.platform_dispatcher().submission();
CE::Text::FontSelection selection;
selection.preferred = {"DejaVu Sans"};
selection.asset_directories = {asset_root_};
selection.automatic_system_fonts = false;
fonts_.emplace(CE::Text::FontCollection::load(selection));
auto layout = CE::Text::layout_text(*fonts_, "Score: 0", {.pixel_height = 24});
auto prepared = CE::Text::Rendering::prepare(std::move(layout));
score_.emplace(CE::Text::Rendering::upload(std::move(prepared), engine_.resources()));
```

Font selection snapshots bytes; layout/prepared values retain those snapshots.
Missing preferred fonts can fall back according to the collection contract.
Explicit application font assets give repeatable appearance across machines.
The saved dispatcher endpoint submits work without capturing a borrowed context.

## Request a changed value

Assign `requested_score_` when game rules change the score. For a simple
extension to First Game, add this after its existing gameplay updates so Space
also increments the score:

```cpp
if (tick.input.button(next_cell_).pressed() && requested_score_ < std::numeric_limits<int>::max())
    ++requested_score_;
```

Use an appropriate bounded score type/policy for the actual game.

## Adopt and prepare without blocking simulation

After handling score changes, run this block during each `update()`:

```cpp
if (pending_score_.valid() && pending_score_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    auto replacement = pending_score_.get();
    score_.emplace(std::move(replacement));
    displayed_score_ = submitted_score_;
}
if (!pending_score_.valid() && requested_score_ != displayed_score_) {
    auto prepared = CE::Text::Rendering::prepare(
        CE::Text::layout_text(*fonts_, std::format("Score: {}", requested_score_), {.pixel_height = 24}));
    pending_score_ = text_platform_.submit(
        [prepared = std::move(prepared)](CE::Runtime::Context& owner) mutable {
            return CE::Text::Rendering::upload(std::move(prepared), owner.resources());
        });
    submitted_score_ = requested_score_;
}
```

Keep drawing the previous complete generation while waiting. A ready exception
propagates before replacing `score_`; this example uses runtime's normal failure
cleanup. An application that catches it must choose its reporting/retry policy.
Do not call `get()` on a still-pending platform dependency from simulation.

Only owned prepared data crosses to graphics. The callback captures neither the
game nor a borrowed string. Small-label layout/preparation runs on simulation;
larger content can use a tracked worker group with the same upload boundary.

One pending request bounds queue growth. Further changes coalesce into the next
replacement, and unchanged values submit no work. For example, if `10` uploads
while the requested value advances through `11` to `12`, adoption displays `10`
and the next request prepares `12`.

## Publish in screen coordinates and release

Keep First Game's compatible text material and image key when submitting:

```cpp
CE::Rendering::DrawStyle2D style;
style.material = text_;
style.model_matrix =
    glm::translate(glm::mat4(1.0f), glm::vec3(24.0f, static_cast<float>(camera_.framebuffer_size().height) - 48.0f, 0.0f));
pass.add(CE::Rendering::Submission::resolve_text(*score_, style, context));
```

Here `context` is the pass's `Context2D` with image parameter `"image"`, as in
[drawing a sprite](draw-sprite.md). The generation owns layout/atlas resources;
changing the source string alone cannot change an uploaded label. A new
generation leaves old frame packets valid.

For a scrolling world, use
[First Game's separate screen pass](../first-game.md#keep-text-in-screen-coordinates)
instead of sharing the world camera.

During `deinit()`, after runtime has settled accepted work and recycled frames:

```cpp
score_.reset();
fonts_.reset();
```

Keep provider/material dependencies alive through the game's other cleanup.
The score should start at zero, update after replacement completes, remain
visible while waiting and preserve screen placement during resize and world
movement. Check both runtime modes; repeated unchanged updates should submit no
replacement uploads.

This is the direct text rendering path. [Create a HUD with TGUI](../create-a/hud.md)
builds score and health widgets, while [the RmlUi GUI guide](create-gui.md)
covers documents and interactive editing. Read
[Unicode layout](../../assets/text-layout.md),
[encoding/source indices](../../assets/text-encoding.md) and
[resource publication](../../resources/resource-residency.md) for detailed limits.
