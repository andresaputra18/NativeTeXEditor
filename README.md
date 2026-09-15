# NativeTeXEditor

NativeTeXEditor is an experimental native TeX/LaTeX editor for scientific and technical writing. It is being developed as a fast, local-first alternative focused on responsive editing, reliable TeX compilation, native PDF preview, and source/PDF synchronization.

The project is currently developed and maintained by a single developer with a background primarily in theoretical physics and data science. It is motivated by practical needs encountered when working with large and technical LaTeX documents.

## Current capabilities

The application already includes:

- native Qt/Scintilla editor foundation
- project and document management
- document recovery support
- direct TeX compilation without requiring `latexmk`
- pdfLaTeX, LuaLaTeX, and XeLaTeX support
- BibTeX and Biber orchestration
- native PDF preview
- SyncTeX source-to-PDF and PDF-to-source navigation
- Windows-first CMake/Ninja build workflow

## Development roadmap

Planned work includes:

- TexLab-based language intelligence
- diagnostics and richer error navigation
- document/history features
- bibliography and Zotero integration
- live preview workflows
- editor and PDF-viewer UX refinement
- packaging and installer work
- broader cross-platform support

The goal is to keep the application architecture modular so that external components such as the PDF backend, language server, and TeX tooling remain replaceable.

## Build

The project currently targets Windows first and uses CMake + Ninja + Qt 6.

```cmd
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
```

A compatible Qt 6 MSVC toolchain and a local TeX distribution are required.

## Repository status and licensing

This repository is publicly visible for project demonstration, technical evaluation, and development transparency.

NativeTeXEditor is currently maintained privately by its author. No open-source license has been selected or granted at this time. Unless explicitly stated otherwise, all rights are reserved.
