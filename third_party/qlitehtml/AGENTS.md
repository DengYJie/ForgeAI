# AGENTS.md — qlitehtml

Agent context for the vendored `qlitehtml` subproject: Qt bindings for the
`litehtml` HTML/CSS engine, adapted to **litehtml v0.10**.

## Layout

All sources live under `src/`; the public headers are exposed to consumers
as `qlitehtmlwidget.h` / `container/container_qpainter.h` (the include
directory is `src/`).

| File | Responsibility |
|---|---|
| `src/container/container_qpainter.h` | Public API: `DocumentContainer`, `DocumentContainerContext` |
| `src/container/container_qpainter_p.h` | Private declarations: `DocumentContainerPrivate`, `Selection`, `Index` |
| `src/container/container_internal.h` | `qlitehtml::internal` namespace: shared helpers + traversal declarations |
| `src/container/container_qpainter.cpp` | Public API + non-painting callbacks (fonts, images, links, media) + resources |
| `src/container/container_painting.cpp` | All draw callbacks (text, list markers, borders, fills, images, gradients) + layer clipping/tiling |
| `src/container/container_selection.cpp` | `Selection`/`Index` + document-tree traversal + hit testing |
| `src/container/container_serializer.cpp` | `DocumentContainer::selectedHtml()` HTML export |
| `src/elements/element_checkbox.*` | Custom `<input type="checkbox">` element, registered via the element factory |
| `src/qlitehtmlwidget.*` | `QLiteHtmlWidget` (scroll area) + `QLiteHtmlSearchWidget` (code-built UI, no .ui file) |
| `src/qlitehtml_global.h` | Export/import macros |

## litehtml v0.10 API Rules

- No `tchar_t` / `tstring` — use `char` / `std::string`.
- `document_container` overrides must match v0.10 signatures exactly:
  - `create_font(const font_description&, const document*, font_metrics*)`
  - `draw_image` / `draw_solid_fill` / `draw_linear_gradient` /
    `draw_radial_gradient` / `draw_conic_gradient` (no `draw_background`)
  - `get_viewport` (not `get_client_rect`)
  - `set_clip(pos, border_radiuses)` (no `valid_x`/`valid_y`)
  - `on_mouse_event(el, mouse_event)` is pure virtual
- `element` children: `children()` returns
  `const std::list<std::shared_ptr<element>>&`; no `get_child(i)` /
  `get_children_count()`.
- `pixel_t` is `float` — brace-init from ints is a narrowing error; use
  `qRound()` (see `toQRect` in `container_internal.h`).
- Hit testing: `document->root_render()->get_element_by_point(x, y, cx, cy, nullptr)`.
- Computed styles: `element->css().get_*()`; fonts via `element->css().get_font()`.
- `document::createFromString(estring, container, masterCss, "")`; the built-in
  `litehtml::master_css` is used when the context has no custom master sheet.

## Rules

- Do not use `QString::fromStdString` / `QString::toStdString` for litehtml
  strings — use `QString::fromUtf8(s.data(), int(s.size()))` and
  `.toUtf8().constData()` (STL debug/release layout differs across the Qt DLL
  boundary).
- No `.ui` files — `QLiteHtmlSearchWidget` builds its UI in code.

## Semantics to Remember

- `a->is_ancestor(b)` is true when `b` is an ancestor of `a`.
- Leaf traversal (`firstLeaf`/`nextLeaf`) never visits non-leaf elements;
  detect body membership with `current->is_ancestor(body)` where
  `body = root()->select_one("body")`.
- `find_styles_changes()` only reports elements whose matched CSS selectors
  changed; custom element state changes produce no redraw box —
  `DocumentContainer::mousePressEvent` repaints the clicked element's box
  after `on_lbutton_down()` returns true.

## Painting / Scrolling Invariants

- `draw_text` receives viewport coordinates; the selection `segmentMap` is
  keyed by document coordinates (reconstruct with `+ m_scrollPosition`).
- Widget painting draws at `-scrollPosition()`; `scrollContentsBy` must call
  `viewport()->scroll(-dx, -dy)`. Fixed elements and the selection highlight
  are repainted via `DocumentContainer::fixedBoxes()` / `selectionRects()`.
- Background layers are clipped to the rounded border box
  (`clipBackgroundLayer`) and tiled per `layer.repeat` (`drawPattern`).
- Gradient coordinates are relative to `layer.origin_box`; CSS conic angle is
  clockwise from top → `QConicalGradient(center, 90.0 - angle)`.

## Search Index

- `buildIndex()` (full, on `setDocument`) and `updateIndex()` (incremental,
  on `appendHtml`) index visible text leaves inside `<body>`.
- `findText()` searches with `QRegularExpression`
  (case/whole-word/backward/wrap). DOM mutations other than `appendHtml`
  require a full `buildIndex()`.

## Custom Elements

`DocumentContainer::registerElementFactory(tag, factory)`; `create_element`
routes through the registry and falls back to `nullptr`. The checkbox is
registered by default for `input[type=checkbox]` (use the `doc` parameter,
not the container's `m_document`). Interactive elements override
`on_lbutton_down()` and return true so the container repaints the element box.

## Verification

No test suite. For changes: build the `qlitehtml` target and the parent
application; run the standalone smoke program covering render, search
(setHtml + appendHtml) and scroll; keep smoke artifacts in the build
directory only.

## Style

- Qt conventions: `QStringLiteral`, pointer-to-member connects, `m_` member
  prefix, `camelCase` functions, `PascalCase` types, snake_case file names.
- C++17 (the subproject's `CXX_STANDARD`).
- Shared internals live in `namespace qlitehtml::internal`
  (`container_internal.h`) — no new globals.
