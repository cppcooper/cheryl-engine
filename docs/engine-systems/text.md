# Text

## Purpose

Text selects font data, validates/decodes text, shapes Unicode runs and prepares
drawable glyphs. Font loading, shaping, rasterization and graphics upload are
different stages with different ownership and execution requirements.

## Interface

Types below are in `CE::Text` unless another namespace is shown.

### Types and operations

| API and purpose |
| --- |
| {cpp:struct}`FontSelection <CE::Text::FontSelection>`, {cpp:class}`FontCollection <CE::Text::FontCollection>`<br>Select ordered fallback fonts and retain immutable font-byte snapshots. [Font selection](../assets/text-layout.md#font-selection); [font discovery](../assets/file-and-font-discovery.md). |
| {cpp:class}`ShapedText <CE::Text::ShapedText>`, {cpp:struct}`LayoutOptions <CE::Text::LayoutOptions>`<br>Run `layout_text()` and inspect shaped lines, glyphs and source mapping. [Shaping, direction and width](../assets/text-layout.md#shaping-direction-and-width). |
| {cpp:class}`Rendering::Prepared <CE::Text::Rendering::Prepared>`, {cpp:class}`Rendering::Generation <CE::Text::Rendering::Generation>`<br>Prepare CPU glyph geometry, upload retained resources and resolve text draw packets. [Preparation and submission](../assets/text-layout.md#preparation-upload-and-retained-submission); [text task](../getting-started/how-to/update-text.md). |
| {cpp:class}`FontRasterizer <CE::Text::FontRasterizer>`, {cpp:struct}`FontRasterGlyph <CE::Text::FontRasterGlyph>`<br>Obtain shared CPU font metrics, raster pixels and kerning for toolkit adapters. [UI font responsibilities](../development/ui-adapters.md#bridge-responsibilities); [rasterizer declarations](../../projects/engine/include/cheryl/text/font-rasterizer.h). |
| {cpp:struct}`Legacy::Font <CE::Text::Legacy::Font>`<br>Use the separate legacy ASCII/bitmap compatibility path. [Legacy bitmap fonts](../resources/legacy-ffont.md). |

### Sources, direction and raster values

| Type | Choices and details |
| --- | --- |
| {cpp:type}`FontSource <CE::Text::FontSource>` | File, system family or string source selection; make discovery policy explicit. [Font sources and fallback](../assets/text-layout.md#font-selection). |
| {cpp:enum}`ParagraphDirection <CE::Text::ParagraphDirection>` | `Automatic`, `LeftToRight`, `RightToLeft` paragraph direction. [Shaping and bidi direction](../assets/text-layout.md#shaping-direction-and-width). |
| {cpp:struct}`SourceRange <CE::Text::SourceRange>`, {cpp:struct}`GlyphId <CE::Text::GlyphId>` | Distinguish source byte/scalar ranges from font-qualified glyph indices. [Encoding and source indices](../assets/text-encoding.md). |
| {cpp:enum}`FontSizing <CE::Text::FontSizing>`, {cpp:enum}`FontBitmapFormat <CE::Text::FontBitmapFormat>` | Raster sizing in `Pixels`/`Characters`; output `Alpha`/`PremultipliedRGBA` pixel storage. [Raster size/format declarations](../../projects/engine/include/cheryl/text/font-rasterizer.h); [toolkit alpha conventions](../development/ui-adapters.md#bridge-responsibilities). |

## Usage

### Uses

Font discovery and CPU processing precede resource upload and draw resolution.
Applications can prepare on Workers and dispatch owned upload requests to
graphics.

| Types | Systems |
| --- | --- |
| {cpp:class}`CE::Text::FontCollection`<br>{cpp:class}`CE::Text::FontRasterizer`<br>{cpp:struct}`CE::Rendering::Resources::Provider`<br>{cpp:class}`CE::Rendering::Resources::Material`<br>{cpp:class}`CE::Runtime::PlatformDispatcher::Submission` | [Assets](assets.md)<br>[Utilities](utilities.md)<br>[Resources](resources.md)<br>[Rendering](rendering.md)<br>[Workers](workers.md)<br>[Dispatch](dispatch.md) |

### Used by

Games draw labels, dialogue and HUD text. Toolkit adapters use font
metrics/rasterization within their own layout; the application selects which
text path to use.

| Types | Systems |
| --- | --- |
| [`Tutorial::Hud`](../getting-started/create-a/hud.md)<br>{cpp:class}`CE::UI::TGUI::Session`<br>{cpp:class}`CE::UI::RmlUi::Session` | [Rendering](rendering.md)<br>[GUI](gui.md) |

## Contracts

Layout positions use local Y-up pixels; `pixel_height` is the font em size.
Glyph IDs are qualified by their retained font collection. Shaping clusters and
UTF-8 byte/scalar ranges are not interchangeable character indices. Layout
retains the font-byte snapshot used for later preparation, not a borrowed input
string or mutable native face.

Fallback and wrapping operate on the documented Unicode boundaries. Replacing
text produces another layout/uploaded generation; it must not mutate a
generation already retained by a frame. Select compatible text materials and
font resources before publishing.

`FontRasterizer::load()` copies font bytes; its copies share synchronized native
face state. Returned metrics/glyph pixels own their values and can survive the
rasterizer. This CPU service neither borrows a graphics provider nor makes a
toolkit's mutable atlas thread-safe.

## Lifecycle

Load selected fonts at a loading boundary, lay out text and prepare its CPU
representation. Upload after graphics initialization and retain the generation
in the game/HUD. Replace it when content changes, allowing published frames to
retain older generations. Settle upload requests and release game handles before
backend shutdown. Retained font bytes and native graphics lifetime are separate.

## Execution

Font selection/layout/preparation are CPU stages; upload follows graphics
affinity. Use owned request data across dispatch boundaries and consume completed
futures without blocking simulation. Record positions and immutable generation
handles during frame preparation.

## Integration

With a previously loaded `fonts` collection on the platform upload owner:

```cpp
auto layout = CE::Text::layout_text(fonts, "Score: 0", {.pixel_height = 24});
auto prepared = CE::Text::Rendering::prepare(std::move(layout));
auto score = CE::Text::Rendering::upload(std::move(prepared), engine_.resources());
```

Retain `score`, resolve it with a compatible material in frame preparation and
replace it through the same ownership stages when the score changes. See
[the text task guide](../getting-started/how-to/update-text.md).

## Limits and further reading

Text rendering is not a full editor/IME service. Do not infer arbitrary toolkit
font compatibility or complete script acceptance from a basic label example.
Read [Unicode layout](../assets/text-layout.md),
[encoding and source indices](../assets/text-encoding.md),
[font discovery](../assets/file-and-font-discovery.md),
[legacy fonts](../resources/legacy-ffont.md),
[layout declarations](../../projects/engine/include/cheryl/text/layout.h),
[shared rasterization operations](../../projects/engine/include/cheryl/text/font-rasterizer.h) and
[text rendering declarations](../../projects/engine/include/cheryl/assets/types/2d/unicode-text.h).
