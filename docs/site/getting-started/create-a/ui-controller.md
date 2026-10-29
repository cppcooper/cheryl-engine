# Create a UIController

Give the game's interface one owner for its TGUI session, input policy, and
retained scene. HUD, menu, and chat components can share that session through
borrowed references. The application defines this `UIController`; game rules
continue to own values such as health and score.

Start with [First Game](../first-game.md), or continue from
[Create a Character](character.md). This lesson establishes the shared lifecycle.
[Create a HUD](hud.md) adds the first visible component.

The enclosing UI owner manages the session's lifetime. Components borrow that
session rather than creating a separate session for each panel:

```mermaid
flowchart TB
    accTitle: A UI owner with one shared session
    accDescr: The game owns its UIController. That controller owns a TGUI session and the components added in later lessons; each component borrows the session while managing its widgets.
    game["Game"] -->|owns| controller["UIController"]
    controller -->|owns| session["Shared TGUI session"]
    controller -->|owns added components| components["UI components"]
    components -->|borrow| session
```

## Select TGUI

Enable `CHERYL_BUILD_UI_TGUI=ON` in First Game's source configuration, or use an
SDK containing that module. In Project setup's copied CMake project, change its
local `set(CHERYL_BUILD_UI_TGUI OFF)` to `ON` before `add_subdirectory()`; those
{ref}`scoped module selections <source-module-selection>` take precedence over a
same-name command-line cache option. Add the UI link to your executable target:

```cmake
target_link_libraries(game PRIVATE Cheryl::UI::TGUI)
```

Use your target's name in place of `game`. Prepare the pinned toolkit through
[Project setup](../project-setup.md). Enabling the module does not download it.
The uploader obtains compatible default materials from the provider. OpenGL embeds
their shaders, so this example needs no additional shader or material files.

## Assign the execution owners

The session runs widget logic, callbacks, layout, and CPU scene recording on the
simulation owner. The platform owner polls native input, creates/uploads graphics
resources, and renders the retained scene. Keeping callbacks with simulation lets
them update UI intent alongside the game state they observe.

| Game hook | Owner | UI work |
| --- | --- | --- |
| `init_platform()` | Platform, before simulation starts. | Configure fonts and construct the controller's uploader from the live provider. |
| `init_simulation()` | Simulation, before Ready and the first update. | Construct the session with automatic view following, receive its initial size, then create widgets. |
| Each `update(tick)` | Simulation. | Runtime applies view changes before the hook; deliver input, advance toolkit time, update components, and publish available scene replacements. |
| `prepare_render_frame(frame)` | Simulation. | Append the latest complete UI scene after the world pass. |
| `deinit_platform()` | Platform, after simulation joins and accepted work settles. | Release component-held toolkit references, then close the session. |

In sequential mode platform and simulation use the same thread. Concurrent mode
uses a dedicated simulation thread. `init_simulation()` supplies the creating
owner and the initial view in both modes. All live session/widget operations use
that owner. Final session closure stays in `deinit_platform()` after UI
quiescence and accepted work have settled.

Constructing an uploader on simulation records that thread as its owner. A
later dispatched platform upload then throws `failed_operation`; its future
rethrows during adoption. Constructing a session on platform in concurrent mode
similarly makes later simulation session calls reject the wrong owner. Graphics
upload and live toolkit work therefore need separate initialization hooks even
when one controller coordinates both.

## Establish the shared session

Create `ui-controller.h` with this initial controller. It has no widgets yet;
keep First Game's existing direct score display until the HUD lesson replaces it.

```cpp
#pragma once

#include <cheryl/core/engine/engine-context.h>
#include <cheryl/core/game-framework/tick-context.h>
#include <cheryl/ui/tgui/scene.h>
#include <cheryl/ui/tgui/session.h>

#include <future>
#include <memory>
#include <optional>

namespace Tutorial {
    class UIController final {
        static constexpr CE::Input::Routing::TargetId tgui_target_ = 2;

        CE::Runtime::Context& engine_;
        CE::Runtime::PlatformDispatcher::Submission platform_;
        CE::UI::TGUI::SceneUploader uploader_;
        CE::UI::TGUI::Scene scene_;
        std::future<CE::UI::TGUI::Scene> pending_;
        std::unique_ptr<CE::UI::TGUI::Session> session_;

    public:
        explicit UIController(CE::Runtime::Context& engine)
        : engine_(engine), platform_(engine.platform_dispatcher().submission()), uploader_(engine.resources()) {}

        void init_simulation() {
            session_ = std::make_unique<CE::UI::TGUI::Session>(
                engine_, tgui_target_, CE::UI::TGUI::SessionOptions{.automatic_view = true}
            );
        }

        void update(const CE::Game::TickContext& tick) {
            session_->handle_input(tick.input.records(), false);
            session_->update_time(tick.delta_seconds);

            static_cast<void>(CE::UI::TGUI::adopt_scene(pending_, scene_));
            if (!pending_.valid())
                pending_ = uploader_.submit(platform_, session_->record());
        }

        void append(CE::Rendering::FrameWriter& frame) const { scene_.write(frame); }

        void close_after_quiescence() {
            if (session_) {
                session_->close_after_quiescence();
                session_.reset();
            }
            scene_ = CE::UI::TGUI::Scene{};
        }
    };
}
```

`tgui_target_` identifies this entire session as a keyboard/text routing recipient.
Any nonzero `TargetId` is suitable; its numeric magnitude gives no priority. Use
distinct identities for independent recipients sharing the input system, such as
TGUI and RmlUi sessions. All menus, chat fields, and HUD widgets in this TGUI
session share its routing identity. TGUI widget focus selects the control within it.
See [session routing and widget focus](../../gui/index.md#session-routing-and-widget-focus).

`automatic_view` applies the runtime's current simulation-view seed before the
session constructor returns. Later size changes apply before the matching game
update, so the controller needs no dimension cache or repeated `set_view()` call.
Logical sizes drive layout/input coordinates; pixel sizes provide framebuffer
scaling and drawable state, including zero extents during minimization.

Time stays manual here: input is delivered before `update_time()`. The separate
`automatic_time` option changes that order to the runtime's pre-update phase;
do not enable it while retaining the manual call. See
[automatic view and time](../../modules/tgui.md#automatic-view-and-time) for
explicit attachment, manual overrides and standalone sessions.

The passive example selects no pointer delivery and requests no keyboard focus.
Feed the full input batch once for session bookkeeping, including empty batches.
The tick's input view is borrowed for this update; retain the controller and
session between calls, using each call's fresh tick.

## Connect the controller to the game

Include `ui-controller.h` in the game header and add a member:

```cpp
std::optional<Tutorial::UIController> ui_;
```

During `init_platform()`, after graphics initialization and any font configuration:

```cpp
ui_.emplace(engine_);
```

Declare `void init_simulation() override;` in the game class, then define it
alongside the other hooks:

```cpp
void Game::init_simulation() {
    ui_->init_simulation();
}
```

For this passive HUD, after quit handling and gameplay changes in `update()`:

```cpp
ui_->update(tick);
```

After world draws in `prepare_render_frame()`:

```cpp
ui_->append(frame);
```

During `deinit_platform()`, after the runtime has joined simulation and settled accepted work:

```cpp
if (ui_)
    ui_->close_after_quiescence();
ui_.reset();
```

The controller retains the latest complete uploaded scene while at most one
replacement is pending. `adopt_scene()` checks readiness without blocking;
failed uploads propagate while the preceding scene remains retained. Live widget
edits do not change that older scene. Keep input, the provider, and dispatch
dependencies alive through runtime cleanup.

For a TGUI HUD plus an independently published RmlUi pause menu, continue with
[Combine a HUD and pause menu](../how-to/pause-menu.md). That procedure moves
interactive UI input before gameplay and keeps UI publication running while paused.

## Add interface components

Construct components after the automatically sized session returns, on simulation.
Pass each a `Session&` whose lifetime covers its widgets and callbacks. The
controller drives input/time and records the whole GUI; individual components
update their own presentation. Each component should remove only its own widgets
when retired during play. Release all component-held toolkit references before
final session closure.

The next lesson adds `Hud` to this controller. Menus and chat can use the same
relationship. Keep application choices about visible panels, modal behavior,
pointer selection, and restoring widget focus in the enclosing UI owner. The
session already supplies event capture and routed keyboard/text focus leases.
For complete interactive input handling, follow the
[TGUI demo](../../../../projects/apps/demo/src/tgui-demo.cpp) and
[focus procedure](../../gui/index.md#session-routing-and-widget-focus).

## Observe the lifecycle

At this checkpoint the UI has no widgets, so its scene is empty. Inspect the
session and `engine_.simulation_view().snapshot()` in a debugger during updates.
The session should be created once and follow logical/pixel size changes. Run both runtime modes
to exercise the owner boundary. Continue with [Create a HUD](hud.md) for visible
score and health widgets and the completed maintained controller.
