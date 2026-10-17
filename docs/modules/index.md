# Modules

Choose modules for the capabilities your application needs, then link their
public CMake targets. Engine supplies portable contracts; concrete modules
implement related responsibilities without becoming dependencies of neutral
Engine consumers. A system name in [Engine Systems](../engine-systems/index.md)
does not imply a separate library.

For the interfaces a replacement must fulfill, use the
[adapter contract catalogue](../architecture/interfaces-adapters.md).
It separates runtime requirements from optional backend capabilities and
application-owned integrations.

| Capability | Owner guide | What it adds |
| --- | --- | --- |
| Application bootstrap | [Startup](startup.md) | Command-line configuration, configured module integration and standard context creation. |
| Worker configuration/topology | [Workers](workers.md) | Optional JSON settings and native topology over Engine-owned pools. |
| Native display and input | [Native GLFW](native-glfw.md) | Window/monitor operations, input collection and native lifetime management. |
| Graphics and presentation | [OpenGL](opengl.md) | Rendering, graphics resources and presentation using Native GLFW. |
| Native widgets | [TGUI](tgui.md) | TGUI widgets, routed input and retained scene publication. |
| Native documents | [RmlUi](rmlui.md) | RML/RCSS documents, routed input and retained scene publication. |
| Audio | [miniaudio](miniaudio.md) | Decoding, device/offline output and streamed playback without window/graphics requirements. |
| Process output viewer | [Linux Debug terminal](debug-terminal-linux.md) | Optional Linux process attachment and output-viewer lifetime. |

These pages publish the canonical guides kept beside each owner's source.
Selection flags, public targets, dependency requirements, standalone composition
and limits are documented with the owner. The
[repository module index](../../projects/modules/README.md) also maps those owners
to the portable contracts they implement.

## Select an assembly

[Assembling the Engine](../architecture/assembling-engine.md) connects the build
facade, public includes and startup factories. Use
[source and SDK consumption](../development/consuming-engine.md) for complete
application CMake and deployment instructions.

Start with [Project setup](../getting-started/project-setup.md) for the supported
source-checkout and installed-SDK paths. Availability in the configured build
and linkage into your application are different choices: an enabled module
becomes usable through its selected public target. Link the published target to
inherit its required usage settings; avoid assembling private support targets
or repeating their implementation libraries in your application.

Native GLFW and OpenGL form a compatible native graphics path. Audio can be
used independently. TGUI and RmlUi are independent integrations; their
application-owned sessions use Engine input/rendering contracts instead of
requiring Native GLFW or OpenGL in their public APIs. Platform-specific process
attachments have different lifetime and deployment needs from ordinary modules.

## Learn or extend

[GUI construction](../gui/index.md) explains application-facing widget/document
integration. [Source and SDK consumption](../development/consuming-engine.md)
covers linking and startup. [Module authoring](../development/modules.md) and
[UI adapter authoring](../development/ui-adapters.md) describe extension rules;
they remain in Development rather than being copied into every owner guide.
