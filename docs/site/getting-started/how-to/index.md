(how-to)=
# Task index

Find a task across the engine documentation and open its instructions directly.
For a complete application, follow [Project setup](../project-setup.md) and
[Tutorials](../create-a/index.md). To browse procedures and implementation
references by subject, use [Guides by topic](../../guides/index.md).

```{contents} Tasks
:local:
:depth: 1
```

(start-configure-and-distribute-a-game)=
## Project setup and startup

| Task | Guide |
| --- | --- |
| Build from source or SDK | [Project setup](../project-setup.md). |
| Use an installed SDK | [Source and SDK integration](../../development/consuming-engine.md#prebuilt-development-sdks). |
| Configure startup and CLI options | [First Game startup](../first-game.md#main-configure-and-start-the-engine); [Startup module](../../modules/startup.md). |
| Choose modules and link targets | [Module selection](../../modules/index.md#select-an-assembly); [granular composition](../../development/consuming-engine.md#granular-composition). |
| Assemble a runtime context | [Context assembly](../../development/consuming-engine.md#context-assembly). |

(input-and-game-state)=
## Input

| Task | Guide |
| --- | --- |
| Add/remove action bindings | [Bind input actions](bind-input.md). |
| Choose an actor's control source | [Actor controllers and commands](../create-a/actor-controller.md). |
| Attach/detach player input | [Runtime consumer contexts](bind-input.md#reuse-bindings-for-runtime-consumers); [PlayerController](../create-a/controller.md) and [Character](../create-a/character.md) tutorials. |
| Drive an actor with AI | [Create an AIController](../create-a/ai-controller.md). |
| Choose a common controller interface | [Abstract controller interfaces](../create-a/abstract-controller.md). |
| Bind axes and scroll | [Axis bindings and control identities](../../runtime/input-state-model.md#control-identities-and-binding-tokens); [controller backend choices](../../modules/native-glfw.md#choose-a-controller-path). |
| Bind key/button chords | [Chord bindings](../../runtime/input-state-model.md#bindings). |
| Read input events and text | [Capture and record channels](../../runtime/input-state-model.md#ordered-events-and-os-text). |
| Manage keyboard focus | [GUI input routing](../../gui/index.md#route-input-and-focus-explicitly); [focus policies](../../runtime/input-state-model.md#focus-routing-and-editing-controls). |
| Read button state and timing | [Tick input](../../runtime/input-state-model.md#tick-input); [input and simulation time](../../runtime/simulation-timing.md#input-and-tick-time). |

(assets-images-and-animation)=
## Assets and animation

| Task | Guide |
| --- | --- |
| Load indexed assets | [Load and publish assets](../../assets/asset-loading.md#runtime-entry-point). |
| Define sprite sheets and pivots | [Add a sprite sheet](../../assets/asset-manifests.md#add-a-sprite-sheet); [pivot choices](../../assets/asset-manifests.md#pivots). |
| Animate sprites | [Play sprite animation](animate-sprite.md). |
| Select and animate tiles | [Application map example](../../assets/asset-values-and-playback.md#sample-an-application-map); [tile playback](../../assets/asset-values-and-playback.md#tile-playback-and-rules). |
| Load assets in the background | [Worker preparation and platform upload](../../assets/asset-loading.md#runtime-entry-point). |
| Reload shaders/materials | [Publication and explicit replacement](../../assets/asset-loading.md#publication-and-retry). This does not reload images or sprites under existing keys. |
| Find files | [File indexing](../../assets/file-and-font-discovery.md#file-index). |
| Install sample artwork | [Artwork packages](../../assets/catalog.md). |

## Drawing and materials

| Task | Guide |
| --- | --- |
| Draw a sprite | [First Game sprite walkthrough](../first-game.md#put-a-sprite-on-screen). |
| Draw a whole image | [Draw a whole image](draw-sprite.md#draw-a-whole-image). |
| Position objects and cameras | [Position objects with a camera](use-camera.md). |
| Keep text/HUD fixed on screen | [A separate screen pass](../first-game.md#keep-text-in-screen-coordinates); [TGUI HUD](../create-a/hud.md). |
| Use default materials | [Default asset materials](../../rendering/pipelines-and-materials.md#default-asset-materials). |
| Set per-object materials/parameters | [Custom sprite material example](../../rendering/pipelines-and-materials.md#give-one-object-a-custom-material). |
| Choose image filtering | [Nearest sampling](../../rendering/pipelines-and-materials.md#choose-nearest-sampling-for-pixel-art). |
| Clip drawing to a rectangle | [Rectangular clipping](../../rendering/pipelines-and-materials.md#rectangular-clipping). |
| Configure pipeline state | [Pipeline parameters and state](../../rendering/pipelines-and-materials.md#parameters-and-ownership); [frame and recipe integration](../../rendering/pipelines-and-materials.md#frame-and-recipe-integration). |
| Release graphics resources | [Cache ownership](../../resources/resource-residency.md#cache-ownership); [native retirement](../../resources/resource-residency.md#native-retirement-and-maintenance). |

(fonts-text-and-gui)=
## Text and fonts

| Task | Guide |
| --- | --- |
| Set default fonts or reuse a FontSelection | [Ordered application defaults](../../assets/text-layout.md#set-an-ordered-application-default). |
| Register named fonts and groups | [Named catalogue choices](../../assets/text-layout.md#register-named-families-and-groups). |
| Save random font choices | [Named groups and saved font references](../../assets/text-layout.md#choose-and-save-a-font). |
| Display/update text | [Display and update text](update-text.md); [retained text updater](../../assets/text-layout.md#retained-text-updates). |
| Select font styles and fallbacks | [Retained styles](../../assets/text-layout.md#retained-font-handles-and-style); [font selection](../../assets/text-layout.md#font-selection). |
| Set wrapping and direction | [Text layout](../../assets/text-layout.md#shaping-direction-and-width). |
| Interpret text encoding/indices | [Encoding and source indices](../../assets/text-encoding.md). |
| Draw contiguous text runs | [Positioned text runs](../../assets/text-layout.md#positioned-text-runs), including its shader and floating-point limits. |
| Use legacy fonts | [Legacy font compatibility](../../resources/legacy-ffont.md). |

## GUI

| Task | Guide |
| --- | --- |
| Choose a GUI backend | [GUI integration choices](../../gui/index.md#choose-an-interface). |
| Own a shared TGUI session | [Create a UIController](../create-a/ui-controller.md). |
| Create a TGUI HUD | [Create a HUD with TGUI](../create-a/hud.md). |
| Create an interactive RmlUi document | [Create an interactive GUI with RmlUi](create-gui.md). |
| Combine a TGUI HUD and RmlUi pause menu | [Pause-menu composition](pause-menu.md). |
| Move keyboard focus between menu and chat | [Session routing and widget focus](../../gui/index.md#session-routing-and-widget-focus). |
| Customize GUI fonts | [GUI font selection](../../gui/index.md#select-fonts). |
| Refresh a GUI view after size changes | [Shared session dimensions](../create-a/ui-controller.md#establish-the-shared-session). |
| Resize GUI layouts | [TGUI layout](../../modules/tgui.md#typed-layout); [RmlUi document layout](create-gui.md#author-the-document-and-stylesheet). |
| Customize UI materials | [Custom UI materials](../../rendering/pipelines-and-materials.md#custom-ui-materials). |

## Audio

| Task | Guide |
| --- | --- |
| Load/play sounds | [miniaudio preparation and output](../../modules/miniaudio.md#preparation-and-output). |
| Stream/control audio playback | [Clips and voices](../../runtime/audio.md#clips-and-voices); [module example](../../modules/miniaudio.md#preparation-and-output). |
| Change audio speed | [Opt into variable-rate playback](../../runtime/audio.md#change-playback-speed). |
| Manage playback lifetime | [Audio lifetime and scheduling](../../runtime/audio.md#lifetime-and-scheduling). |

(windows-timing-and-work-delivery)=
## Windows and displays

| Task | Guide |
| --- | --- |
| Resize windows/change window modes | [Window sizes and modes](../../runtime/display-and-window-contract.md#sizes-modes-and-observation). |
| Handle display scaling | [Display dimensions](../../runtime/display-and-window-contract.md#sizes-modes-and-observation); [scale and UI](../../runtime/display-and-window-contract.md#scale-and-ui-capabilities). |

## Runtime and timing

| Task | Guide |
| --- | --- |
| Implement the game lifecycle | [Game hooks](../first-game.md#game-state-and-lifecycle); [runtime lifecycle](../../runtime/runtime-frame-boundary.md). |
| Configure simulation timing | [Simulation timing](../../runtime/simulation-timing.md). |
| Shut down the runtime | [Runtime shutdown](../../runtime/runtime-frame-boundary.md); [ownership and cleanup](../../architecture/ownership-cleanup.md). |

## Background work

| Task | Guide |
| --- | --- |
| Run background CPU jobs | [Worker jobs](../../runtime/worker-execution.md#jobs); [context-owned groups](../../runtime/worker-execution.md#context-ownership). |
| Configure worker pools/CPU placement | [Typed/JSON worker settings](../../runtime/worker-execution.md#typed-and-json-configuration); [CPU policy](../../runtime/worker-execution.md#cpu-policy-and-capabilities). |
| Dispatch platform/simulation work | [Thread dispatch](../../runtime/thread-dispatch.md). |
| Publish prepared replacements | [Prepare and publish a replacement](../../architecture/threads-data-flow.md#prepare-and-publish-a-replacement). |

## Events

| Task | Guide |
| --- | --- |
| Find engine events or send a signal | [Engine-event catalogue](../../engine-systems/events.md#engine-events); [named signals](../../runtime/event-delivery.md#named-channels). |
| Create and send typed events | [Create an event](../create-a/event.md); [channel contracts](../../runtime/event-delivery.md#typed-channels). |
| Queue event delivery | [Queued event delivery](../../runtime/event-delivery.md#optional-queued-delivery). |
| Remove event listeners | [Registration removal](../../runtime/event-delivery.md#removal-and-shutdown). |

(memory-and-cpu-utilities)=
## Memory, state and utilities

| Task | Guide |
| --- | --- |
| Choose a state representation | [State representation choices](../../runtime/utility-contracts.md#choose-a-state-representation); [transition tables](../../runtime/utility-contracts.md#transition-tables). |
| Pool application objects | [Object-pool ownership](../../resources/resource-lifetime.md#pool-ownership). |
| Reserve/release pooled slots | [Reservations](../../resources/resource-lifetime.md#reservations). |
| Measure time/convert durations | [Time helpers](../../runtime/utility-contracts.md#measure-elapsed-time). |
| Parse numbers | [Numeric parsing](../../runtime/utility-contracts.md#parse-numbers). |
| Size allocations | [Allocation sizing](../../runtime/utility-contracts.md#size-cpu-allocations). |
| Pack bits | [Packed bits](../../runtime/utility-contracts.md#store-packed-bits). |
| Build geometry | [Geometry helpers](../../runtime/utility-contracts.md#build-geometry). |
| Manage memory blocks | [Block and pointer helpers](../../runtime/utility-contracts.md#retain-blocks-and-format-values). |

(logging-diagnostics-and-failures)=
## Logging and diagnostics

| Task | Guide |
| --- | --- |
| Configure/troubleshoot logging | [Logging setup](../../runtime/logging.md#enable-useful-output); [missing/delayed records](../../runtime/logging.md#investigate-missing-or-delayed-records). |
| Inspect subsystem activity | [Investigate a runtime symptom](../../runtime/subsystem-diagnostics.md#investigate-a-runtime-symptom). |
| Trace retained graphics resources | [Trace retained resource owners](../../runtime/subsystem-diagnostics.md#trace-retained-resource-owners). |
| Handle/report failures | [Failure reporting](../../runtime/failure-reporting.md). |
| Open the Linux output viewer | [Linux output viewer](../../modules/debug-terminal-linux.md). |

## Engine development

| Task | Guide |
| --- | --- |
| Produce an SDK | [SDK production](../../development/building.md#development-sdk-builds). |
| Write a module | [Module authoring](../../development/modules.md#add-a-module). |
| Write a UI adapter | [UI adapter authoring](../../development/ui-adapters.md). |
| Run engine tests | [Testing and validation](../../development/testing.md). |
| Generate documentation | [Documentation tooling](../../development/documentation.md). |
| Publish documentation | [Publishing](../../development/publishing-documentation.md). |
| Run CI and collect SDKs | [CI and SDK artifacts](../../development/ci.md). |

See [Engine Systems](../../engine-systems/index.md) for responsibilities and public
types, and [the capability inventory](../../../STATUS.md) for implementation status.
