(modules)=
# Choose modules

Choose modules for the capabilities your application needs, then link their
public CMake targets. Engine supplies portable contracts; concrete modules
implement related responsibilities without becoming dependencies of neutral
Engine consumers. A system name in [Engine Systems](../engine-systems/index.md)
does not imply a separate library.

For the interfaces a replacement must fulfill, use the
[Integration contracts](../architecture/interfaces-adapters.md).
It separates runtime requirements from optional backend capabilities and
application-owned integrations.

| Capability | Owner guide | What it adds |
| --- | --- | --- |
| Application bootstrap | [Application startup reference](startup.md) | Command-line configuration, configured module integration and standard context creation. |
| Worker configuration/topology | [Worker configuration and topology](workers.md) | Optional JSON settings and native topology over Engine-owned pools. |
| Native display and input | [Native GLFW platform reference](native-glfw.md) | Window/monitor operations, input collection and native lifetime management. |
| Graphics and presentation | [OpenGL backend reference](opengl.md) | Rendering, graphics resources and presentation using Native GLFW. |
| Native widgets | [TGUI reference](tgui.md) | TGUI widgets, routed input and retained scene publication. |
| Native documents | [RmlUi reference](rmlui.md) | RML/RCSS documents, routed input and retained scene publication. |
| Audio | [miniaudio reference](miniaudio.md) | Decoding, device/offline output and streamed playback without window/graphics requirements. |
| Process output viewer | [Linux output viewer](debug-terminal-linux.md) | Optional Linux process attachment and output-viewer lifetime. |

Topic guides explain portable Engine workflows; these references document the
concrete modules used to implement them. These pages own the module documentation;
the README beside each module's code is a symlink to its reference here. Selection
flags, public targets, dependency requirements, standalone composition and limits
are documented in each reference. The
[repository module index](../../../projects/modules/README.md) also maps those owners
to the portable contracts they implement.

## Select an assembly

[Source and SDK integration](../development/consuming-engine.md) connects public
targets, includes, startup factories and application deployment.

Start with [Project setup](../getting-started/project-setup.md) for the supported
source-checkout and installed-SDK paths. Availability in the configured build
and linkage into your application are different choices: an enabled module
becomes usable through its selected public target. Link the published target to
inherit its required usage settings: include directories, feature definitions,
C++ requirements and transitive link dependencies. Avoid assembling private support targets
or repeating their implementation libraries in your application.

Native GLFW and OpenGL form a compatible native graphics path. Audio can be
used independently. TGUI and RmlUi are independent integrations; their
application-owned sessions use Engine input/rendering contracts instead of
requiring Native GLFW or OpenGL in their public APIs. Platform-specific process
attachments have different lifetime and deployment needs from ordinary modules.

## Learn or extend

[GUI construction](../gui/index.md) explains application-facing widget/document
integration. [Module authoring](../development/modules.md) and
[UI adapter authoring](../development/ui-adapters.md) describe extension rules;
they belong to Engine Development rather than being copied into every owner guide.
