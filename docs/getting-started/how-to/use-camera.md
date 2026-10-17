# Position objects with a camera

A camera supplies projection and view matrices; the game supplies object
transforms and chooses which camera each pass uses. Start from
[drawing a sprite](draw-sprite.md). This task separates moving an object in world
space from changing the view of that world.

## Track drawable dimensions

Keep a simulation-owned `CE::Rendering::Camera2D camera_`. During initialization
on platform, it may read `engine_.window().framebuffer_size()`. During update:

```cpp
camera_.set_framebuffer_size(tick.framebuffer_size);
```

`Camera2D` projects Y-up framebuffer-pixel bounds from zero to width/height.
Logical window dimensions can differ on scaled displays. Negative pixel sizes
reject; zero sizes are retained while projection clamps each axis to at least
one. The camera does not turn logical input coordinates into pixels for you.

## Move the object

With `<glm/gtc/matrix_transform.hpp>`, place the sprite through its model matrix:

```cpp
style.model_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(position_, 0.0f));
```

The asset's pivot defines which local point reaches `position_`. Style scale
changes local geometry; it does not change the meaning of your world position.
Select your game's coordinate convention explicitly when extending beyond this
pixel-oriented example.

## Change the view

For a view whose world-space origin is `view_origin_`, update the camera on its
owner:

```cpp
camera_.set_view_matrix(glm::translate(glm::mat4(1.0f), glm::vec3(-view_origin_, 0.0f)));
```

Moving the origin right shifts the displayed world left without changing any
entity's authoritative position. Keep `view_origin_` as application-owned
`glm::vec2` state. This is explicit positioning; a following-camera behavior
would choose that state according to its own game policy.

## Copy matrices into passes

During frame preparation:

```cpp
auto world = frame.begin_pass(camera_.projection_matrix(), camera_.view_matrix());
```

Populate `world` with resolved sprite/tile packets. A second HUD camera using an
identity view can begin a later pass so world scrolling does not move screen
information. Both writers borrow the current frame; retain neither them nor
camera matrix references beyond preparation.

Camera revisions count changed values; they are not atomic publication. Runtime
transfers the copied matrices with the completed frame and renderer playback
does not read your live camera.

## Observe the result

Resizing should update visible pixel bounds. Changing `position_` moves just the
actor; changing `view_origin_` moves the world view. A separate HUD pass remains
anchored to its own projection. Keep all camera mutations in update and release
the game's resource handles during ordinary cleanup; cameras own CPU values and
need no backend shutdown call.

Read [camera contracts](../../rendering/pipelines-and-materials.md#camera-observations),
[display dimensions](../../runtime/display-and-window-contract.md#sizes-modes-and-observation)
and [frame ownership](../../runtime/runtime-frame-boundary.md). Perspective
parameters belong to `Camera3D`; they do not automatically select depth policy
or a scene system.
