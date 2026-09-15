# NativeTeXEditor

NativeTeXEditor is an experimental native TeX/LaTeX editor focused on fast local editing,
project workflows, direct TeX compilation, native PDF preview, and SyncTeX navigation.

## Current status

Phase 4 complete.

Implemented so far:
- native Qt/Scintilla editor foundation
- project and document management
- recovery support
- direct pdfLaTeX / LuaLaTeX / XeLaTeX compilation
- BibTeX / Biber orchestration
- native PDF preview
- SyncTeX source/PDF navigation

Next development phase: TexLab language intelligence.

## Build

The project currently targets Windows first and uses CMake + Ninja + Qt 6.

```cmd
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
```

This repository is publicly viewable for project demonstration and evaluation.
No open-source license is granted at this time.
