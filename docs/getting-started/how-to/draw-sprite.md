# Load and draw a sprite

This task loads the maintained First Game actor and publishes it through a
camera pass. Complete [Project setup](../project-setup.md) and run
[First Game](../first-game.md) first so its assets, shaders and matching resource
provider are available. The result is one retained sprite at a chosen position.

## Load on the platform owner

Include the loader, sprite manager, shader manager, submission and camera
headers used in [the maintained source](../../../projects/apps/first-game/src/first-game.cpp).
During `init()`, load the asset root through the initialized provider:

```cpp
CE::Assets::Loading::Loader loader(asset_root_);
loader.load_assets(engine_.resources());
sprite_ = CE::Assets::Sprites::Manager::get().get_asset("first-game", "actor");
images_ = CE::Assets::Shaders::Manager::get().find_material("first-game:images");
if (!sprite_ || !images_)
    throw std::runtime_error("The actor and image material are required");
```

Store `sprite_` as `std::shared_ptr<const CE::Assets::Sprites::Asset>` and
`images_` as `std::shared_ptr<const CE::Rendering::Resources::Material>`. The
[actor definition](../../../projects/apps/first-game/assets/graphics/definitions/sprites.json)
describes two cells; its image path is relative to the enclosing `graphics`
directory, independent of the definition file's nesting. The
[graphics index](../../../projects/apps/first-game/assets/graphics/graphics-manifests.json)
selects that definition and the material recipes.

Loading can publish some entries before a later upload fails. Treat failure as
failed initialization and run partial-safe cleanup, following the
[loading contract](../../assets/asset-loading.md#publication-and-retry).

## Keep visible state in the game

Keep a `glm::vec2 position_{160.0f, 100.0f}`, a
`CE::Assets::Grids::CellIndex cell_ = 0` and `CE::Rendering::Camera2D camera_`
alongside those handles. During update:

```cpp
camera_.set_framebuffer_size(tick.framebuffer_size);
```

Change position and cell there according to game state. Cells are zero-based
grid indices; choose one below `sprite_->definition().grid.cell_count()`.
Use the copied tick dimensions in concurrent mode instead of reading the window.

## Resolve a packet during frame preparation

In `prepare_render_frame(CE::Rendering::FrameWriter& frame) const`:

```cpp
auto pass = frame.begin_pass(camera_.projection_matrix(), camera_.view_matrix());
const CE::Rendering::Submission::Context2D context{
    pass.semantics(), pass.parameters(), pass.constraints(),
    CE::Rendering::Submission::ImageParameter2D{"image", 0}
};
CE::Rendering::DrawStyle2D style;
style.material = images_;
style.scale = 4.0f;
style.model_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(position_, 0.0f));
pass.add(CE::Rendering::Submission::resolve_sprite(*sprite_, cell_, style, context));
```

Include `<glm/gtc/matrix_transform.hpp>` for translation. The image parameter
key matches the material's public sampler key; it is not a native uniform name.
Style scale enlarges local geometry, while the model transform positions the
actor relative to its pivot. The camera uses framebuffer pixels and Y-up bounds.

Resolution performs CPU work and retains the selected geometry/material/image
generation in the packet. Do not issue graphics calls or retain the borrowed
pass writer. Runtime handles publication and presentation.

## Release and observe

Reset the game's sprite/material handles during `deinit()`, after simulation has
stopped and while the backend remains alive. Frame packets keep their own copies
until recycling; a retained C++ handle cannot draw through a destroyed backend.

The actor should appear at the chosen position and scale. If it does not, check
asset identity, selected cell, material sampler key and camera dimensions before
changing backend code. Continue with [animation](animate-sprite.md),
[camera placement](use-camera.md) or [a Player class](../create-a/player.md).
Detailed packet and lifetime rules remain in
[render submission](../../rendering/pipelines-and-materials.md) and
[residency](../../resources/resource-residency.md).
