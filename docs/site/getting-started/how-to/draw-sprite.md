(load-and-draw-a-sprite)=
# Draw a whole image

Use `CE::Assets::Graphics::Asset` to draw a background or other whole PNG without
defining a sprite sheet. It retains the uploaded image, a matching quad and a
compatible default material. No graphics index, sprite definition or application
shader files are required for this path.

Start from [First Game](../first-game.md), which owns the
[sprite walkthrough](../first-game.md#put-a-sprite-on-screen) and
[drawing pass setup](../first-game.md#understand-what-a-draw-needs).
This procedure adds a background behind that game's actor.

## Keep visible state in the game

Include these headers alongside First Game's context, camera and frame headers:

```cpp
#include <cheryl/assets/types/2d/graphic.h>
#include <cheryl/assets/resources/resource-provider.h>
#include <cheryl/assets/submission/draw2d.h>
#include <cheryl/core/resources/asset-management/texture-mgr.h>
#include <glm/gtc/matrix_transform.hpp>
#include <optional>
#include <stdexcept>
```

Retain an application-owned member alongside `actor_` and `camera_`:

```cpp
std::optional<CE::Assets::Graphics::Asset> background_;
```

Reuse First Game's camera initialization and its update from
`tick.framebuffer_size`. During concurrent updates, use that copied size rather
than reading the native window.

## Load on the platform owner

Put `background.png` in the application asset root. First Game's existing
`loader.load_assets()` uploads standalone PNGs as well as indexed sprite images.
After that call in platform-owned `init_platform()`, retrieve its image and create the quad:

```cpp
const auto background_file = std::filesystem::absolute(asset_root_ / "background.png").lexically_normal();
auto image = CE::Assets::Images::Manager::get().get_asset(background_file);
if (!image)
    throw std::runtime_error("The background image is required");
background_.emplace(CE::Assets::Graphics::Asset::from_image(image, engine_.resources()));
```

The image cache uses the resolved, lexically normalized path as its key.
`from_image()` reuses that uploaded image, uploads the quad and obtains the
provider's default textured material. It requires a nonnull image with nonzero
pixel dimensions from the same provider domain.

When loading an image without the asset loader, use this alternative in
platform-owned `init_platform()` instead:

```cpp
background_.emplace(CE::Assets::Graphics::Asset::load(
    asset_root_ / "background.png", engine_.resources()));
```

`load()` decodes and uploads the image before creating the quad. Use one of
these paths for a given background, keeping uploads on the provider owner;
frame preparation only reads the retained result. Creation failure throws, so
cleanup must also tolerate an empty `background_`. Neither creation path
publishes a graphic asset in a manager cache.

## Resolve a packet during frame preparation

The default pivot is the image's top-left corner. With First Game's identity
camera view, place that corner at the camera's upper-left and submit before
the foreground actor. Replace its existing pass/context declarations with this
block, keeping the actor and text draws after it:

```cpp
auto pass = frame.begin_pass(camera_.projection_matrix(), camera_.view_matrix());
const CE::Rendering::Submission::Context2D context{
    pass.semantics(), pass.parameters(), pass.constraints()
};
CE::Rendering::DrawStyle2D background_style;
background_style.model_matrix = glm::translate(
    glm::mat4(1.0f), glm::vec3(0.0f, static_cast<float>(camera_.framebuffer_size().height), 0.0f));
pass.add(CE::Rendering::Submission::resolve_graphic(*background_, background_style, context));
```

The camera uses framebuffer pixels and Y-up bounds. This places the image at
its native pixel size; it does not stretch on resize. Draw order determines
which later sprites cover it. For a fixed background with a scrolling world,
use [a separate screen pass](../first-game.md#keep-text-in-screen-coordinates).

Resolution uses the graphic's retained material and inserts its image under the
standard sampler key. An explicit `background_style.material` selects an
override; see the [per-object material example](../../rendering/pipelines-and-materials.md#give-one-object-a-custom-material)
for loading a material and supplying parameters.

Resolution performs CPU work and retains geometry, material and image handles
in the packet. Do not issue graphics calls or retain the borrowed pass writer.
Runtime handles publication and presentation.

## Release and observe

During `deinit_platform()`, release this game's use alongside its other handles:

```cpp
background_.reset();
```

Earlier packets retain their image, geometry and material until their frame is
recycled. Keep the backend alive through those frames' completion.

The background should appear at the upper-left behind the actor. Resize the
window to confirm the camera and placement update while the image keeps its
native size. Continue with [camera placement](use-camera.md) for a scrolling
view. Detailed packet and lifetime rules remain in
[render submission](../../rendering/pipelines-and-materials.md) and
[residency](../../resources/resource-residency.md).
