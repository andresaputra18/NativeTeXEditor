# Third-party dependencies

NativeTeXEditor uses pinned upstream editor-engine versions:

```text
Scintilla 5.6.6
Lexilla   5.5.3
```

Official source archives:

```text
https://www.scintilla.org/scintilla566.zip
https://www.scintilla.org/lexilla553.zip
```

## Current Phase 1 build behavior

If these directories exist, CMake uses them directly:

```text
third_party/scintilla/
third_party/lexilla/
```

Otherwise `cmake/Dependencies.cmake` fetches the exact pinned source archives
into the CMake build tree and compiles them as static libraries.

This network access is development/build-time only. It is not part of the
runtime product and will not be required by end users.

## Licensing

Scintilla and Lexilla use permissive licenses that permit use in free and
commercial products. When the sources are vendored for the shipping tree, keep
their upstream `License.txt` files with the source.
