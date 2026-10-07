# Neutral UI consumer probe

The Engine-owned `ui_probe.retained_scene` acceptance case uses the existing portable
runtime graph with recording display, input, resources and rendering. It links
Engine alone and exercises both sequential and concurrent runtime modes. It is a
test consumer, without widget/toolkit APIs in the Engine library.

The scene records two overlapping translucent colored panels, scrolling content with
an intersected rectangular clip, a focused editing target, an image and an ASCII
label. Published draws retain their geometry, material, parameters and logical clip
extent in authored order. Playback maps the clip to a resized framebuffer; its
matrices and resources do not refer back to mutable UI state.

Exclusive keyboard focus routes committed `Hi` text to the editing target while
controller State continues to reach gameplay. The observed physical records retain
their order and focus epoch. Simulation handles input and prepares frames; the
platform owner renders and uploads resources. A replacement image is submitted to
the platform and adopted after its future becomes ready, without blocking simulation.
Retained earlier packets keep the original image through replacement and teardown.

The provider copies transient vertex, image and atlas storage. The probe supplies
deterministic synthetic ASCII bake data to the real `STBFont` layout/submission API;
it needs no host font file. Recording establishes these controlled contracts, rather
than native pixels or a particular toolkit's font capabilities.

The OpenGL-owned `native_opengl.ui_clipping_color` case separately checks actual
vertex colors, straight-alpha overlap, scaled top-left scissor coordinates, empty
clips and state reset between draws and full clears. It requires the native opt-in
and a usable display. Its scope differs from physical-device, DPI/compositor,
toolkit or Unicode/IME acceptance.

## Focused validation

After explicit build/test authorization, reuse suitable Engine-only and OpenGL build
directories. Build `tests-engine`, `acceptance-engine` and `consumer-cengine` together
in Engine only; build `tests-opengl`, `acceptance-opengl` and
`consumer-module-opengl` together in OpenGL. Use one low-priority job and cooling
breaks. The [architecture guide](architecture-validation.md) describes their options
and isolation checks. Consumer targets also build their first-include probes.

Run the Engine unit runner once, then select the recording graph's acceptance:

```sh
./build-engine/tests-engine
./build-engine/tests-acceptance-engine --gtest_filter='ui_probe.*:runtime_adapter.*'
./build-opengl/tests-opengl
CHERYL_NATIVE_GL_TESTS=1 ./build-opengl/tests-acceptance-opengl \
  --gtest_filter='native_opengl.ui_clipping_color'
```

Do not repeat the same cases through owner/cross-project aggregates. Run serially
in isolated working directories; a native skip does not establish pixel acceptance.
The [render contract](../rendering/pipelines-and-materials.md#rectangular-clipping)
defines clipping/color behavior and the [adapter-author guide](ui-adapters.md)
defines the independent toolkit boundaries and acceptance scope.
