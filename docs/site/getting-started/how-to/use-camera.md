# Position objects with a camera

A camera supplies projection and view matrices; the game supplies object
transforms and chooses which camera each pass uses. Start from
[First Game's sprite walkthrough](../first-game.md#put-a-sprite-on-screen).
This task separates moving an object in world space from changing the view of
that world.

## Track drawable dimensions

Reuse First Game's simulation-owned `CE::Rendering::Camera2D camera_`. During
initialization on platform, it reads `engine_.window().framebuffer_size()`.
Keep its existing update:

```cpp
camera_.set_framebuffer_size(tick.framebuffer_size);
```

`Camera2D` projects Y-up framebuffer-pixel bounds from zero to width/height.
Logical window dimensions can differ on scaled displays. Negative pixel sizes
reject; zero sizes are retained while projection clamps each axis to at least
one. The camera does not turn logical input coordinates into pixels for you.

## Move the object

First Game retains the position in `actor_->position` and already includes
`<glm/gtc/matrix_transform.hpp>`. Its frame preparation places the sprite through
the model matrix:

```cpp
image_style.model_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(actor_->position, 0.0f));
```

The asset's pivot defines which local point reaches `actor_->position`. Style scale
changes local geometry; it does not change the meaning of your world position.
Select your game's coordinate convention explicitly when extending beyond this
pixel-oriented example.

## Change the view

Add a `glm::vec2 view_origin_{};` game member for the world-space origin of the
view. For a visible first step, set it to `(80, 0)` during `update()` and then
update the camera there:

```cpp
view_origin_ = glm::vec2(80.0f, 0.0f);
camera_.set_view_matrix(glm::translate(glm::mat4(1.0f), glm::vec3(-view_origin_, 0.0f)));
```

The actor should appear 80 pixels farther left than in the unchanged view,
while its stored position stays the same. After that checkpoint, replace the
fixed assignment with the view origin chosen by your game's movement policy.

Moving the origin right shifts the displayed world left without changing any
entity's authoritative position. This is explicit positioning; a following-camera
behavior would choose that state according to its own game policy.

## Copy matrices into passes

Keep First Game's pass setup during frame preparation:

```cpp
auto pass = frame.begin_pass(camera_.projection_matrix(), camera_.view_matrix());
```

Its `context` and draws still use this `pass`. Move direct screen text to
[First Game's separate HUD pass](../first-game.md#keep-text-in-screen-coordinates)
so world scrolling does not move it. Both writers borrow the current frame;
retain neither them nor camera matrix references beyond preparation.

Camera revisions count changed values; they are not atomic publication. Runtime
transfers the copied matrices with the completed frame and renderer playback
does not read your live camera.

## Observe the result

Resizing should update visible pixel bounds. Changing `actor_->position` moves
just the actor; changing `view_origin_` moves the world view. A separate HUD pass
remains anchored to its own projection. After initialization, keep camera
mutations in update and release the game's resource handles during ordinary
cleanup; cameras own CPU values and need no backend shutdown call.

Read [camera contracts](../../rendering/pipelines-and-materials.md#camera-observations),
[display dimensions](../../runtime/display-and-window-contract.md#sizes-modes-and-observation)
and [frame ownership](../../runtime/runtime-frame-boundary.md). Perspective
parameters belong to `Camera3D`; they do not automatically select depth policy
or a scene system.
