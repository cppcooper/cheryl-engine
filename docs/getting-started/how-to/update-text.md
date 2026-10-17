# Display and update text

Text crosses three stages: CPU layout/preparation, graphics upload, then CPU
frame submission. Start with [First Game](../first-game.md), which already has
the matching `first-game:text` material. This task changes a label without doing
graphics work on concurrent simulation or mutating resources retained by an
earlier frame.

## Load fonts and an initial generation

Include the font collection, layout, Unicode text and submission headers used
by [the maintained example](../../../projects/apps/first-game/src/first-game.cpp).
During platform-owned initialization:

```cpp
CE::Text::FontSelection selection;
selection.preferred = {"DejaVu Sans"};
selection.asset_directories = {asset_root_};
selection.automatic_system_fonts = false;
auto fonts = CE::Text::FontCollection::load(selection);
auto layout = CE::Text::layout_text(fonts, "Score: 0", {.pixel_height = 24});
auto prepared = CE::Text::Rendering::prepare(std::move(layout));
auto initial = CE::Text::Rendering::upload(std::move(prepared), engine_.resources());
```

Retain `fonts` for replacements and move `initial` into an optional
`CE::Text::Rendering::Generation` member. Selection snapshots font bytes; missing
preferred fonts can fall back according to the collection contract. Explicit
application font assets give repeatable appearance across machines.

## Prepare a replacement only when content changes

Keep the requested score, the displayed/submitted score and at most one
`std::future<CE::Text::Rendering::Generation> pending_`. Include `<future>`,
`<chrono>` and `<format>`. During update, when no request is pending and content
differs from the displayed value:

```cpp
auto replacement = CE::Text::Rendering::prepare(
    CE::Text::layout_text(fonts_, std::format("Score: {}", requested_score_), {.pixel_height = 24}));
pending_ = engine_.platform_dispatcher().submit(
    [prepared = std::move(replacement)](CE::Runtime::Context& owner) mutable {
        return CE::Text::Rendering::upload(std::move(prepared), owner.resources());
    });
submitted_score_ = requested_score_;
```

Only owned prepared data crosses to graphics; the callback does not capture the
HUD or a borrowed string. Layout/preparation is CPU-only. For larger text, run
that CPU stage through a tracked worker group and retain the same upload
boundary. The simple label needs no unbounded queue of intermediate scores.

## Adopt without blocking simulation

Before considering another submission in update:

```cpp
if (pending_.valid() && pending_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    auto replacement = pending_.get();
    score_.emplace(std::move(replacement));
    displayed_score_ = submitted_score_;
}
```

Keep drawing the previous complete generation while waiting. A ready exception
propagates before replacing `score_`; decide whether the application reports it
and retries or allows runtime's normal failure cleanup. If the requested value
changed while upload waited, submit the latest value after adopting this one.
Do not call `get()` on a still-pending platform dependency from simulation.

## Publish and release

In frame preparation, use a compatible text material and image key:

```cpp
CE::Rendering::DrawStyle2D style;
style.material = text_;
style.model_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(24.0f, 280.0f, 0.0f));
pass.add(CE::Rendering::Submission::resolve_text(*score_, style, context));
```

Here `context` is the pass's `Context2D` with image parameter `"image"`, as in
[drawing a sprite](draw-sprite.md). The generation owns layout/atlas resources;
changing the source string alone cannot change an uploaded label. A new
generation leaves old frame packets valid.

During cleanup reset the game-owned generation after runtime has settled
accepted work and recycled frames. Keep provider/material dependencies alive
until then. The score should update after replacement completes, remain visible
while waiting and preserve correct placement across runtime modes.

[Create a HUD](../create-a/hud.md) supplies the complete class and coalescing
state. Read [Unicode layout](../../assets/text-layout.md),
[encoding/source indices](../../assets/text-encoding.md) and
[resource publication](../../resources/resource-residency.md) for detailed limits.
