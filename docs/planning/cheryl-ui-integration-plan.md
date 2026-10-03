# Cheryl Engine UI Integration Strategy and Development Plan

## Purpose

This document defines a development strategy for making Cheryl Engine capable of supporting multiple GUI/widget libraries without coupling the engine core to any particular UI framework.

The immediate motivation is practical:

- Cheryl needs a usable game HUD/menu/widget system.
- RmlUi is architecturally attractive and capable of producing highly polished interfaces, but its RML/RCSS authoring model inherits many of the complexities of HTML/CSS.
- For a solo developer who is much stronger in C++ than web UI design, a C++-first toolkit such as TGUI may be substantially easier to use during early development.
- If the project later becomes financially successful, RmlUi becomes especially attractive because a dedicated CSS/RML UI specialist could build sophisticated production interfaces without requiring Cheryl to change its engine architecture.
- Dear ImGui may also be useful later for engine/debug/developer tooling, but it should remain a separate concern from player-facing UI.

The goal is therefore **not to choose one GUI library for Cheryl Engine**. The goal is to make Cheryl expose the correct generic facilities so that UI libraries can be integrated as optional modules.

---

# 1. Strategic Direction

## 1.1 Core principle

`cheryl-engine` should not depend on TGUI, RmlUi, Dear ImGui, CEGUI, Nuklear, or any other GUI/widget library.

Instead, the intended dependency direction is:

```text
                         Game
                          |
              +-----------+-----------+
              |                       |
        Cheryl::UI::TGUI        Cheryl::UI::RmlUi
              |                       |
              +-----------+-----------+
                          |
                    Cheryl Engine
                 +--------+--------+
                 |        |        |
               Input   Rendering  Platform
                         Resources
```

A dependent game chooses which UI integration modules it needs.

For example:

```cmake
target_link_libraries(my_game PRIVATE
    Cheryl::Engine
    Cheryl::UI::TGUI
)
```

A later game could instead use:

```cmake
target_link_libraries(my_game PRIVATE
    Cheryl::Engine
    Cheryl::UI::RmlUi
)
```

A project could deliberately link both during a migration or because different libraries serve different purposes.

The important rule is:

```text
game -> UI adapter/module -> cheryl-engine
```

Never:

```text
cheryl-engine -> GUI library
```

---

# 2. Why Multiple UI Libraries Should Be Supported

## 2.1 RmlUi

RmlUi is a particularly strong long-term option because it is designed to integrate into an existing engine rather than replace its windowing, rendering, or event loop.

Its strengths include:

- retained-mode UI
- sophisticated layout
- CSS-like styling
- animations and transitions
- flexbox
- templates
- localization
- data binding
- custom elements
- game-focused integration
- renderer abstraction
- platform abstraction
- proven use in polished applications and games

Its main disadvantage for the current development context is authoring complexity.

RML/RCSS gives substantial expressive power, but robust CSS layout can become difficult around cases such as:

- tooltips near viewport boundaries
- dynamically sized popovers
- context menus
- dropdowns
- constrained text wrapping
- responsive HUD placement
- nested flex layouts
- dynamic sizing and overflow
- floating UI anchored to moving objects

These are solvable, but solving them well can require real web-layout expertise.

For a solo C++ developer, that is a meaningful development cost.

## 2.2 TGUI

A C++-first toolkit such as TGUI is potentially a better initial UI implementation because its widget creation and positioning model maps more directly onto C++ reasoning.

The likely near-term role is:

```text
TGUI
    first production-capable player UI
    C++ authored
    practical for solo development
```

The likely later role for RmlUi is:

```text
RmlUi
    polished production UI
    sophisticated visual design
    potentially authored by a dedicated UI/CSS specialist
```

This should not be treated as a hard replacement path. Both integrations can remain available.

## 2.3 Dear ImGui

Dear ImGui should be treated separately.

Its natural role is:

```text
engine/debug UI
profilers
resource inspectors
render inspectors
developer consoles
editor panels
diagnostic overlays
```

It should not define Cheryl's player-facing GUI architecture.

---

# 3. What Cheryl Should Abstract

Cheryl should abstract the facilities a UI library consumes.

It should **not** abstract the widgets themselves.

Good abstraction boundaries include:

- render submission
- textures/resources
- input events
- text input
- focus and routing
- platform services
- clipboard
- cursor control
- DPI/content scaling
- lifecycle and threading

Bad abstraction boundaries include:

```cpp
Cheryl::Button
Cheryl::Panel
Cheryl::Dropdown
Cheryl::TextBox
Cheryl::TreeView
```

Creating a universal Cheryl widget API would make Cheryl itself into a UI toolkit and would force fundamentally different libraries toward a lowest-common-denominator interface.

TGUI users should be able to use TGUI's normal API.

RmlUi users should be able to use RML/RCSS and RmlUi's C++ API.

Dear ImGui users should be able to use ImGui directly.

The commonality belongs **below the widget layer**.

---

# 4. Rendering Strategy

UI libraries must not issue OpenGL, Vulkan, or other backend API commands directly.

Instead:

```text
UI library
    |
library-specific render adapter
    |
Cheryl render commands / RenderFrame
    |
Renderer
    |
OpenGL / Vulkan / future backend
```

For example, an RmlUi integration would implement `Rml::RenderInterface`, but that implementation should translate RmlUi draw requests into Cheryl render-frame data rather than issuing OpenGL calls.

Likewise, a TGUI integration should translate TGUI rendering into Cheryl's renderer-facing primitives.

The generic UI rendering facilities will likely need to support:

- indexed geometry
- vertex colors
- textured geometry
- transforms
- rectangular clipping/scissor
- ordered rendering
- alpha blending

More advanced libraries may eventually require:

- stencil/mask clipping
- render-to-texture
- custom shader/effect requests
- filters
- offscreen surfaces

These should be added when a real integration demonstrates that they are needed rather than being guessed in advance.

The graphics/backend thread should remain the only place where graphics API calls are performed.

This preserves Cheryl's concurrent render model:

```text
simulation / UI preparation
          |
      RenderFrame
          |
graphics/backend thread
          |
OpenGL / Vulkan
```

---

# 5. Resource Strategy

A UI integration should not create backend graphics resources directly.

It should request resources through Cheryl.

The desired flow is:

```text
RmlUi / TGUI
      |
UI resource adapter
      |
Cheryl resource system
      |
renderer/backend
```

This matters for:

- textures
- dynamically generated textures
- texture updates
- font atlases
- render targets
- destruction/lifetime
- concurrent frame ownership

RmlUi may perform its own font rasterization while still handing generated texture data to Cheryl.

TGUI may have different expectations.

The engine should support both approaches without exposing the graphics backend to the UI library.

---

# 6. Input Strategy

Cheryl's input model should remain independent of UI frameworks.

The useful distinction is between:

```text
state-oriented input
ordered physical events
semantic text input
```

UI libraries generally need event-style input rather than only frame state.

The engine therefore needs enough generic information for adapters to translate:

```text
Cheryl key event
    -> RmlUi / TGUI key event

Cheryl text input
    -> RmlUi / TGUI text input

Cheryl pointer movement
    -> RmlUi / TGUI mouse movement

Cheryl button event
    -> RmlUi / TGUI button event

Cheryl wheel event
    -> RmlUi / TGUI scrolling
```

A UI adapter should never need to read GLFW or Gainput directly.

---

# 7. Input Routing and Focus

Supporting multiple UI systems is primarily an **input-routing problem**, not a rendering problem.

The engine may eventually have several consumers:

```text
gameplay
game UI
modal UI
console
developer UI
debug overlay
```

The input system records what physically happened.

A separate routing/focus layer determines who is allowed to react.

That routing layer needs concepts such as:

- keyboard focus
- text-input focus
- pointer capture
- modal interception
- event propagation
- event consumption
- controller-navigation ownership
- priority/order between consumers

One UI library should never need direct knowledge of another UI library.

For example:

```text
RmlUi textbox:
    consumes keyboard and text input

Gameplay:
    may still receive controller state

Modal pause menu:
    suppresses gameplay actions

ImGui debug overlay:
    receives mouse/keyboard only while actively capturing them
```

This separation also makes coexistence between TGUI and RmlUi practical.

---

# 8. Platform Services

UI libraries commonly need platform facilities beyond rendering and input.

Cheryl should expose library-neutral access to services such as:

- monotonic time
- clipboard read/write
- cursor visibility
- cursor shape
- window dimensions
- framebuffer dimensions
- DPI/content scale
- text-input lifecycle
- IME support where available

The generic UI integration path should not require GLFW handles or GLFW headers.

If some platform capability is not available, Cheryl should make that limitation explicit rather than silently implementing incomplete behavior.

---

# 9. Module and Dependency Structure

The likely organization is:

```text
cheryl-engine
    core/
    rendering/
    input/
    resources/
    platform/
    ...

optional UI integrations
    ui/tgui/
    ui/rmlui/
    ui/imgui/
```

These can initially live in the Cheryl repository as optional modules and later move into separate companion repositories/packages if that becomes useful.

Candidate CMake targets:

```text
Cheryl::Engine
Cheryl::UI::TGUI
Cheryl::UI::RmlUi
Cheryl::UI::ImGui
```

Requirements:

- `Cheryl::Engine` links none of the UI libraries.
- Each adapter pulls only its own dependency graph.
- Each adapter can be enabled independently.
- Installing/building Cheryl without GUI support requires no GUI dependencies.
- A game can deliberately link more than one adapter.

This prevents a TGUI-based game from automatically inheriting RmlUi and its dependencies, or vice versa.

---

# 10. Adapter Pattern

Each GUI integration will probably contain roughly the same categories of adapter code:

```text
platform bridge
input translator
render translator
resource translator
lifecycle wrapper
```

The implementations remain completely library-specific.

For example:

```text
RmlUi module
    RmlRenderInterface
    RmlSystemInterface
    RmlInputAdapter
    RmlResourceBridge

TGUI module
    TguiRendererBridge
    TguiInputAdapter
    TguiPlatformBridge
    TguiResourceBridge
```

The existence of similar responsibilities does **not** automatically justify introducing a shared `iUiSystem`.

A common runtime abstraction should only be introduced if actual implementations demonstrate a meaningful common contract.

---

# 11. Application Architecture for Future Migration

The application should keep gameplay/domain state separate from widget implementations.

Prefer:

```cpp
struct InventoryModel;
struct HudState;
struct DialogueState;
struct SettingsModel;
```

Then a TGUI implementation can bind to those models:

```cpp
class TguiInventoryView;
```

A later RmlUi implementation can bind to the same state:

```cpp
class RmlInventoryView;
```

Avoid placing concrete widget types directly inside gameplay/domain objects:

```cpp
tgui::Button
Rml::Element
```

This makes a later transition from TGUI to RmlUi a presentation-layer replacement rather than a gameplay-system rewrite.

---

# 12. Likely Development Path

## Phase A — Engine capability

Do not start by integrating a UI library.

First ensure Cheryl has the generic facilities required by an unknown UI library:

```text
render submission
resource ownership
input events
semantic text input
input routing/focus
platform services
optional module packaging
```

## Phase B — First practical UI

Integrate the first C++-friendly player-facing library, likely TGUI.

Use it to validate the engine boundaries.

Any place where the adapter must bypass Cheryl and reach directly into GLFW/OpenGL/etc. should be treated as an architectural defect worth reviewing.

## Phase C — Backend-independence proof

Verify that the integration:

- exposes no OpenGL types publicly
- exposes no GLFW types publicly
- performs no graphics calls from simulation/UI code
- produces data compatible with the `RenderFrame` pipeline
- would remain structurally valid if Cheryl later gained a Vulkan renderer

## Phase D — RmlUi

When useful, implement RmlUi independently.

This becomes both:

- a production-quality UI option
- a proof that Cheryl actually supports multiple UI libraries

RmlUi should use Cheryl adapters rather than its built-in GLFW/OpenGL backend.

## Phase E — Specialist-authored production UI

If a Cheryl-based game later generates enough revenue to justify specialist UI work, a developer/designer with strong CSS/RML expertise can build the polished player-facing interface in RmlUi.

At that point, the engine should already have all required integration support.

The specialist works primarily at the presentation layer rather than modifying Cheryl's platform/rendering architecture.

## Phase F — Developer tooling

Optionally integrate Dear ImGui for:

- debugging
- profiling
- asset inspection
- console tooling
- renderer inspection
- editor functionality

Its presence should not affect the player-facing UI choice.

---

# 13. Development Task List

- [ ] **1. Define the UI integration contract**
  - [ ] Document the architectural rule: `cheryl-engine` must not depend on TGUI, RmlUi, ImGui, or another widget library.
  - [ ] Define the desired dependency direction:
    - [ ] `game -> cheryl-ui-<library> -> cheryl-engine`
    - [ ] Never `cheryl-engine -> UI library`.
  - [ ] Define which responsibilities belong to Cheryl:
    - [ ] render-command submission
    - [ ] input collection
    - [ ] input routing/focus infrastructure
    - [ ] GPU/resource creation
    - [ ] platform/window services
    - [ ] lifecycle/threading rules
  - [ ] Define which responsibilities remain library-specific:
    - [ ] widget hierarchy
    - [ ] layout
    - [ ] styling
    - [ ] library event model
    - [ ] library-specific widget APIs
  - [ ] Explicitly reject a universal Cheryl widget abstraction such as `Cheryl::Button`, `Cheryl::Panel`, etc.
  - [ ] Do not introduce an `iUiSystem` or `UiLayer` abstraction unless implementation work exposes a genuine common runtime contract.

- [ ] **2. Finish the generic render-submission boundary**
  - [ ] Ensure UI code can contribute rendering without calling OpenGL or another graphics API directly.
  - [ ] Define the render primitives needed by generic UI integrations.
    - [ ] indexed geometry
    - [ ] vertex colors
    - [ ] textured geometry
    - [ ] transforms
    - [ ] rectangular clipping/scissor
    - [ ] ordered rendering
    - [ ] alpha blending
  - [ ] Determine how advanced features are represented when required.
    - [ ] stencil/mask clipping
    - [ ] render-to-texture
    - [ ] shader/effect requests
    - [ ] filters
  - [ ] Ensure UI rendering can be recorded into `RenderFrame`.
  - [ ] Ensure the graphics/backend thread remains the only code issuing backend API calls.
  - [ ] Define ordering relative to other rendering.
    - [ ] world
    - [ ] HUD/UI
    - [ ] overlays
    - [ ] developer/debug UI
  - [ ] Verify that adding multiple UI systems does not require modifying the renderer for each library.
  - [ ] Acceptance criterion: an unknown subsystem can submit everything needed for a basic 2D GUI using only Cheryl render facilities.

- [ ] **3. Complete the UI-relevant resource boundary**
  - [ ] Ensure UI integrations can request textures without direct GPU API access.
  - [ ] Define appropriate resource ownership/lifetime semantics for UI-created resources.
  - [ ] Support dynamic texture creation where a UI library requires it.
  - [ ] Support texture updates if required.
  - [ ] Determine how fonts interact with Cheryl resources.
    - [ ] library-managed font rasterization
    - [ ] Cheryl-managed font resources
    - [ ] both, where appropriate
  - [ ] Ensure destruction occurs while the appropriate graphics context/device is valid.
  - [ ] Ensure UI resources can safely cross the simulation/render-frame boundary.
  - [ ] Acceptance criterion: a UI adapter can create and use images/fonts without knowing which rendering backend Cheryl uses.

- [ ] **4. Finish the generic input model required by UI consumers**
  - [ ] Preserve Cheryl's separation between:
    - [ ] state-oriented input
    - [ ] ordered physical events
    - [ ] semantic text input
  - [ ] Ensure UI consumers can receive mouse/pointer information.
    - [ ] absolute position
    - [ ] relative movement where useful
    - [ ] button transitions
    - [ ] wheel/scroll
  - [ ] Ensure UI consumers can receive keyboard information.
    - [ ] physical key transitions
    - [ ] modifiers
    - [ ] key repeat where appropriate
  - [ ] Ensure UI consumers can receive semantic text independently of physical key state.
  - [ ] Preserve event ordering where the active capture contract requires it.
  - [ ] Ensure consuming UI input does not alter the underlying physical-input record.
  - [ ] Acceptance criterion: an adapter can translate Cheryl input into another library's input/event API without consulting GLFW/Gainput directly.

- [ ] **5. Design input routing and focus independently from input collection**
  - [ ] Define how multiple consumers can coexist.
    - [ ] gameplay
    - [ ] game UI
    - [ ] modal UI
    - [ ] console
    - [ ] developer/debug UI
  - [ ] Define keyboard focus semantics.
  - [ ] Define pointer/mouse capture semantics.
  - [ ] Define modal interception.
  - [ ] Define propagation/consumption semantics.
  - [ ] Define controller-navigation ownership.
  - [ ] Ensure one UI library never needs awareness of another UI library.
  - [ ] Support cases such as:
    - [ ] RmlUi textbox owns keyboard/text input while controller gameplay continues
    - [ ] modal menu suppresses gameplay actions
    - [ ] developer UI overlays the game UI
    - [ ] two independently integrated UI systems coexist
  - [ ] Keep routing policy above the low-level input backend.
  - [ ] Acceptance criterion: consumer priority/focus can change without reconfiguring physical input collection.

- [ ] **6. Expose library-neutral platform services**
  - [ ] Identify services UI libraries commonly require.
    - [ ] monotonic time
    - [ ] clipboard read/write
    - [ ] cursor visibility
    - [ ] cursor shape
    - [ ] framebuffer/window dimensions
    - [ ] DPI/content scale
    - [ ] text-input/IME lifecycle where supported
  - [ ] Place each service under the appropriate Cheryl platform/display abstraction.
  - [ ] Avoid exposing GLFW handles through the generic API.
  - [ ] Make unavailable capabilities explicit rather than silently emulating them incorrectly.
  - [ ] Acceptance criterion: a UI library's platform/system interface can be implemented without including GLFW headers.

- [ ] **7. Define the optional module/package boundary**
  - [ ] Choose the physical organization for integrations.
    - [ ] in-tree optional modules initially, or
    - [ ] separate companion repositories/packages later
  - [ ] Establish target naming such as:
    - [ ] `Cheryl::UI::TGUI`
    - [ ] `Cheryl::UI::RmlUi`
    - [ ] `Cheryl::UI::ImGui`
  - [ ] Ensure `Cheryl::Engine` links none of them.
  - [ ] Ensure each integration brings only its own dependency graph.
  - [ ] Make UI integrations independently enableable in CMake.
  - [ ] Ensure installing/building Cheryl without GUI support requires no GUI dependencies.
  - [ ] Allow a dependent game to link more than one integration deliberately.
  - [ ] Acceptance criterion: a game using only TGUI does not fetch/build/link RmlUi, and vice versa.

- [ ] **8. Define a common adapter pattern without defining a common widget API**
  - [ ] Establish the responsibilities every integration is likely to implement.
    - [ ] engine/platform bridge
    - [ ] input translator
    - [ ] render translator
    - [ ] resource translator
    - [ ] lifecycle wrapper
  - [ ] Keep those implementations library-specific.
  - [ ] Avoid forcing identical public APIs across UI libraries.
  - [ ] Allow TGUI users to use TGUI's native C++ API.
  - [ ] Allow RmlUi users to use RML/RCSS and RmlUi's native C++ API.
  - [ ] Allow ImGui users to use ImGui directly.
  - [ ] Document which Cheryl facilities adapter authors should consume.
  - [ ] Acceptance criterion: adding another UI library requires a new module, not changes to Cheryl core.

- [ ] **9. Implement the first integration as the architectural proof**
  - [ ] Use the UI library selected for initial development—likely TGUI if C++ authoring remains the priority.
  - [ ] Implement its platform bridge.
  - [ ] Implement Cheryl-to-library input translation.
  - [ ] Implement library-to-`RenderFrame` translation.
  - [ ] Implement its texture/resource bridge.
  - [ ] Implement resize/DPI handling.
  - [ ] Implement focus/input-routing participation.
  - [ ] Build a representative UI rather than merely a button demo.
    - [ ] menu
    - [ ] nested panels
    - [ ] dynamic labels
    - [ ] tooltip near viewport boundaries
    - [ ] text field
    - [ ] scrolling
    - [ ] image/texture
  - [ ] Identify any places where the integration has to bypass Cheryl abstractions.
  - [ ] Treat every such bypass as an engine-boundary defect to review.
  - [ ] Do not generalize first-integration quirks into Cheryl core unless they represent genuinely generic requirements.

- [ ] **10. Prove backend independence**
  - [ ] Audit the first adapter for graphics-backend leakage.
  - [ ] Ensure no OpenGL types appear in its public interface.
  - [ ] Ensure no GLFW types appear in its public interface.
  - [ ] Ensure no direct graphics calls occur from simulation/UI-update code.
  - [ ] Verify its generated data can survive the concurrent `RenderFrame` pipeline.
  - [ ] Document any requirements that would affect a future Vulkan renderer.
  - [ ] Acceptance criterion: replacing `OpenGLRenderer` should not require redesigning the UI adapter's public architecture.

- [ ] **11. Implement a second integration as the extensibility proof**
  - [ ] Once useful, integrate RmlUi independently.
  - [ ] Implement `Rml::RenderInterface` against Cheryl rather than RmlUi's OpenGL backend.
  - [ ] Implement `Rml::SystemInterface` against Cheryl platform services.
  - [ ] Translate Cheryl event/text input into RmlUi context input.
  - [ ] Route RmlUi-generated textures/resources through Cheryl where appropriate.
  - [ ] Record RmlUi rendering into `RenderFrame`.
  - [ ] Do not make the TGUI adapter a dependency of the RmlUi adapter.
  - [ ] Run both systems in the same application as an architectural exercise.
  - [ ] Verify focus/input ordering between them.
  - [ ] Acceptance criterion: both libraries coexist while `cheryl-engine` remains unaware of either library.

- [ ] **12. Add optional developer-tool UI support later**
  - [ ] Evaluate Dear ImGui separately from player-facing UI.
  - [ ] Reuse the same generic rendering/input/platform infrastructure.
  - [ ] Keep developer overlays independently enableable.
  - [ ] Verify an ImGui overlay can sit above TGUI or RmlUi without interfering with normal focus unless explicitly active.
  - [ ] Use this as another test that the routing system handles heterogeneous consumers rather than special-casing game UI.

- [ ] **13. Establish application-side migration practices**
  - [ ] Keep gameplay/domain state separate from concrete widget objects.
  - [ ] Prefer models such as:
    - [ ] `HudState`
    - [ ] `InventoryModel`
    - [ ] `SettingsModel`
    - [ ] `DialogueState`
  - [ ] Let each presentation implementation bind to those models independently.
  - [ ] Avoid storing `tgui::*`, `Rml::*`, etc. inside gameplay/domain objects.
  - [ ] Keep library-specific callbacks at the presentation/controller boundary.
  - [ ] Document this as the recommended pattern for games that may migrate UI implementations later.
  - [ ] Acceptance criterion: rebuilding a screen in RmlUi replaces its presentation layer, not the gameplay system behind it.

- [ ] **14. Document the final extension contract**
  - [ ] Add an architecture document explaining that Cheryl is GUI-library agnostic.
  - [ ] Document dependency direction.
  - [ ] Document threading rules.
  - [ ] Document render submission.
  - [ ] Document input/event/text routing.
  - [ ] Document platform-service access.
  - [ ] Document resource ownership.
  - [ ] Provide a minimal "integrating another UI library" guide.
  - [ ] Include one complete adapter diagram.
  - [ ] Record deliberate non-goals:
    - [ ] no universal widget hierarchy
    - [ ] no mandatory UI dependency
    - [ ] no GUI-library types in Cheryl core
    - [ ] no direct backend API access from UI modules
    - [ ] no assumption that only one UI consumer exists

---

# 14. Milestone Interpretation

The most important sequencing rule is:

```text
1–7:
    establish generic Cheryl boundaries

9:
    prove those boundaries with the first UI integration

11:
    prove the architecture is genuinely multi-library
```

The UI work should therefore be folded into the existing renderer/input/runtime architecture work rather than treated as an independent GUI refactor.

Much of what UI support requires is already desirable for Cheryl on its own:

- backend-neutral render submission
- safe resource lifetime
- semantic text input
- ordered input events
- focus/routing
- platform abstraction
- graphics-thread ownership

UI integration should act as a strong consumer-driven test of those systems.

---

# 15. Final Architectural Target

The target architecture is:

```text
                              Game
                               |
          +--------------------+--------------------+
          |                    |                    |
   Cheryl::UI::TGUI     Cheryl::UI::RmlUi    Cheryl::UI::ImGui
          |                    |                    |
          +--------------------+--------------------+
                               |
                         Cheryl Engine
       +-----------------------+-----------------------+
       |                       |                       |
     Input                 Rendering                Platform
       |                       |                       |
       +------------------- Resources ----------------+
                               |
                         Backend layer
                               |
                    OpenGL / future Vulkan
```

The core architectural test is:

> Could a completely unknown future UI library be integrated by implementing adapters against Cheryl's existing rendering, resource, input, routing, and platform APIs without modifying Cheryl core?

If the answer is yes, Cheryl supports multiple UI libraries correctly.

If integrating a new library requires adding that library's concepts or types to `cheryl-engine`, the abstraction boundary is in the wrong place.
