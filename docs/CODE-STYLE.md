# C++ code style

The root `.clang-format` is the formatting authority for C and C++ code. It began with the CLion code-style export and adds a small number of explicit repository conventions where the inherited LLVM defaults disagreed with the codebase. Keeping the file at the repository root allows CLion and command-line `clang-format` to discover it automatically.

## Formatting

- Use four spaces and never tabs for new or edited code.
- Use attached pointer and reference declarators: `Texture* texture` and `const GridDefinition& grid`.
- Keep opening braces on the declaration or control-statement line.
- Let the root formatter decide enum wrapping; short enums may fit on one line. Do not add comments to namespace-closing braces.
- Short inline accessors may remain on one line; keep out-of-line function definitions expanded.
- Treat 140 columns as the limit, matching the root configuration for clang-format 23+. Let `clang-format` decide whether a declaration or expression fits on one line; do not manually align continuation lines.
- Preserve meaningful include groups. In a source file, put its matching header first, then other project or third-party headers, then standard-library headers, with blank lines between groups.

Format individual files with:

```sh
clang-format -i path/to/file.cpp
```

To restrict formatting to lines changed from a base revision, use:

```sh
git clang-format <base-revision>
```

## Code conventions

- The project targets C++23. Prefer the standard library and current language features already used by the surrounding subsystem.
- New headers use `#pragma once`; do not add a second include guard.
- Prefer nested namespaces such as `namespace CE::Assets` and indent their contents.
- Types and enum values use `PascalCase`. Functions, methods, and local variables use `snake_case`.
- Private data members introduced in modernized code use a trailing underscore, such as `root_path_`. Preserve established public APIs and legacy subsystem names rather than renaming them only for style.
- Mark converting constructors `explicit`, and use `override` or `final` where the relationship is known.
- Put data declarations above methods. Keep declaration and definition signatures on one line when they fit within 140 columns; wrap longer parameter lists as a block. Put constructor initializers on the following line, as specified by the root formatter configuration.
- Add `[[nodiscard]]` to query or factory functions when silently discarding the result is probably a mistake.
- Include what a file uses instead of relying on transitive includes.
- Keep comments focused on invariants, ownership, coordinate systems, or other non-obvious intent. Avoid narrating code that is already clear.

## Scope and tests

The repository contains older code written under several conventions. Apply the current style to new code and to lines materially changed by a task; avoid drive-by formatting of unrelated files. Do not reformat vendored code under `extern/` or generated build output.

Tests should follow the production subsystem they cover and use short, intuitive `snake_case` suite and test names. Let the suite identify the subsystem and name each case for its main scenario, usually in two to four words: `input_routing.stale_focus_lease`, `opengl_upload.atlas_upload_failure`. Keep detailed behavior in the test body instead of writing sentence-length names.
