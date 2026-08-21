# Contributing to qlitehtml

Guidelines for the vendored `qlitehtml` subproject (Qt bindings for litehtml
v0.10). Read `AGENTS.md` in this directory for the API rules and invariants.

## Scope

Accepted:

- Bug fixes for rendering, selection, search, scrolling or resource handling.
- Support for additional litehtml v0.10 features (draw callbacks, custom
  elements via the factory registry).
- Behavior-preserving refactors.

Not accepted:

- Whole-repo syncs with upstream — this fork's API and file layout have
  diverged.
- Reintroducing `.ui` files, `SIGNAL`/`SLOT` string connects, or license
  headers.
- `QString::fromStdString` / `toStdString` for litehtml strings (see AGENTS.md).

## Building

The subproject builds as part of the parent application (CMake target
`qlitehtml`); it is not a standalone CMake project.

```sh
cmake --build build/debug --target qlitehtml   # from the parent repo root
cmake --build build/debug                       # full application
```

## Verification

No automated test suite; verify changes manually:

1. `qlitehtml` target builds without errors.
2. Render: load representative HTML (headings, styled text, lists, gradient,
   table, checkbox) in the smoke program; confirm expected output.
3. Search: `findText` succeeds on fresh `setHtml` content and after
   `appendHtml` (incremental index).
4. Scroll: scroll the document and confirm content shifts correctly.
5. The parent application builds and launches.

Keep smoke artifacts in the build directory; never commit them.

## Code Style

- C++17; Qt conventions (`QStringLiteral`, `m_` members, `camelCase`
  functions, `PascalCase` types, snake_case file names, pointer-to-member
  connects).
- File responsibilities are fixed (see AGENTS.md): painting →
  `src/container/container_painting.cpp`; selection/index/traversal →
  `src/container/container_selection.cpp`; HTML export →
  `src/container/container_serializer.cpp`; shared internals →
  `src/container/container_internal.h` under `qlitehtml::internal`.
- No new cross-file globals; shared helpers go to `container_internal.h`.
- Preserve comments explaining non-obvious behavior; add similar comments for
  new non-obvious logic.

## Commit & PR

- Conventional commits: `fix(qlitehtml): ...`, `feat(qlitehtml): ...`,
  `perf(qlitehtml): ...`, `refactor(qlitehtml): ...`.
- One concern per commit; explain what and why in the body.
- Before opening a PR: build passes, verification above passes, no build
  artifacts or smoke outputs committed.

## Upgrading litehtml

If the parent project bumps the litehtml FetchContent tag:

- Diff `include/litehtml/document_container.h` against every override in
  `container_qpainter_p.h` — signature drift breaks the build; semantic
  changes (coordinate spaces, redraw reporting) break silently.
- Check `types.h` typedefs (`pixel_t`, `string`, `elements_list`) and re-run
  the verification suite.
- Expect churn in `container_painting.cpp` if draw callbacks change (as in the
  0.9 → 0.10 migration: `draw_background` → per-layer callbacks).
