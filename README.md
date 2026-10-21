# QtNotebook — a OneNote replacement, minus the cloud and the clutter

Qt Widgets / C++17 notes app built to spec v1
(`~/workspace/goals/onenote-like-qt-notes-app/files/feature-spec-v1.md`).
One notebook = one local file. No OneDrive, no account, no silent loss:
saves are atomic with a backup, and every open runs an integrity check.

Status: **scaffold** — the storage layer and model are real and tested;
the editor UI is a working skeleton with the seams marked TODO.

## Build & run

```sh
cmake -S . -B build && cmake --build build
QT_QPA_PLATFORM=offscreen ./build/qtnotebook_smoke   # headless test, expect SMOKE PASS
./build/qtnotebook                                    # the app
./build/qtnotebook some-notebook.qnb                  # open a file (future double-click association)
```

Requires Qt 6 Core/Gui/Widgets/PrintSupport. **QtPdf is optional:**
CMake looks for it QUIETly — with it, PDF import renders for real;
without it (e.g. this VM's Qt 6.4.2 base packages), the app builds and
PDF import reports honestly that QtPdf is unavailable.

## The .qnb format

A `.qnb` file is a ZIP archive (stored entries, no compression) with a
custom extension. Rename one to `.zip` and any unzip tool opens it.
The directory structure inside IS the notebook:

```
notebook.json                                index: format version, notebook
                                             title, sections[{id,title,dir,
                                             pages[{id,title,file,mediaDir}]}]
<Section Title>/<Page Title>.json            one page: {format:"qtnote-page",
                                             pageId, title, elements[...]}
<Section Title>/<Page Title>.media/<files>   that page's images and
                                             exploded PDF page PNGs
```

- Page **GUIDs** (in the index) are what internal links target
  (`note://page/<guid>`), so renaming or moving a page never breaks a link.
- If `notebook.json` is missing/corrupt, loading **rebuilds the notebook by
  scanning the folders** (new GUIDs are generated — the report says so).
- Elements: `{"id","kind":"text","x","y","w","h","document":{...}}` where
  `document` is the lossless TextDocumentJson form, and
  `{"id","kind":"image","x","y","w","h","media":"map.png"}`.

### Saving — no silent loss

`NotebookPackage::saveAtomic()`: write `<file>.tmp`, reopen and verify the
entry list, copy the previous file to `<file>.bak`, then replace. Any
failure leaves the previous good file untouched and surfaces a loud error.

### Integrity check

`NotebookPackage::integrityCheck()` returns pages found, media found,
missing entries (including media referenced by an image element but absent),
and **broken internal links** (page JSON scanned for `note://page/<guid>`
hrefs whose GUID is not in the index). The result shows in the status bar.

## Layout of the source

```
src/main.cpp               argv[1] notebook open / new untitled
src/mainwindow.*           tree | canvas window, menus, Back/Forward history
src/pagecanvas.*           QGraphicsView, expanding scene, zoom,
                           click-empty-canvas creates a text element
src/notebook.*             Notebook > Section > Page model (content as
                           element JSON + media bytes; scene-free on purpose)
src/notebookpackage.*      .qnb read/write, atomic save, integrity, sanitize
src/zipstore.*             minimal stored-entries ZIP writer/reader + CRC32
                           (self-contained; no private Qt zip headers)
src/pdfexplode.*           PDF -> per-page 300 dpi PNGs (real with QtPdf,
                           reporting stub without); >10 pages = warning
src/elements/textelement.* QGraphicsTextItem + QTextDocument, transparent
src/elements/imageelement.* QGraphicsPixmapItem + media file name
src/textdocumentjson.*     COPIED from ~/workspace/qtnotes/src/ (the Sept 2026
                           lossless QTextDocument <-> JSON serializer);
                           copied, not moved — qtnotes stays untouched and
                           this tree is self-contained.
tests/smoke.cpp            headless round-trip + integrity + rebuild test
```

Code style: no anonymous namespaces, `type& var` / `type* var`, east-coast
braces — the user's conventions, throughout.

## What's real vs stubbed (honest list)

Real and smoke-tested on Qt 6.4.2:
- ZIP read/write (also validated against Python's `zipfile`), atomic save
  with `.bak`, integrity check incl. broken-link detection, index rebuild
  from folder scan, full save/load round trip of formatted text + images.
- The copied TextDocumentJson serializer compiles and round-trips here —
  this also retires its old "reviewed, not compile-tested" caveat.
- Notebook model, tree navigation, Back/Forward history, expanding canvas,
  click-to-type text elements, image insert, zoom, page export to PDF
  (single-pass scene render).

Stubbed / TODO:
- **PDF explode without QtPdf**: menu action surfaces "QtPdf not available
  in this build". With QtPdf the rendering code is written (QPdfDocument,
  300 dpi, >10-page warning) but is **reviewed-only — never compiled**, this
  VM has no QtPdf. The original PDF keeping (`_original.pdf` in the page's
  `.media/`) is a marked TODO at the call site.
- **Internal link clicking** in the canvas: anchors are stored and checked,
  clicking one does not navigate yet (TODO in textelement.cpp).
- **Search**: menu item present but disabled (spec v1 item, after scaffold).
- **Export PDF**: renders the whole scene in one pass; proper pagination
  of the expanding canvas is a TODO. RTF export not started.
- Dialogs use QFileDialog static helpers (desktop-fine); the Android/WASM
  async-dialog pass is later, per spec. No nested `QDialog::exec()` flows.
- No tags, no forms/scripts, no cloud anything — by spec, not by omission.

## Next steps

1. Compile the QtPdf path on a Qt that ships it; wire `_original.pdf`.
2. Anchor-click navigation + Insert Link page picker.
3. Full-text search UI (title + snippet, click to jump).
4. Export pagination; RTF export from the qtnotes framework.
5. Pick the real file extension (`.qnb` is a placeholder) and register it
   in the installer (Qt Installer Framework, as with TableMap).
