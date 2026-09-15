# Central dependency declaration for NativeTeXEditor.
#
# Phase 1 pins the editor engine to:
#   Scintilla 5.6.6
#   Lexilla   5.5.3
#
# If exact sources are later placed in third_party/scintilla and
# third_party/lexilla, those are preferred. Otherwise CMake fetches the pinned
# official source archives into the build tree. End users will never perform
# this fetch; this is a development/build concern only.

include(FetchContent)

find_package(Qt6 6.11 REQUIRED COMPONENTS Widgets Pdf PdfWidgets)
find_package(Qt6 6.11 QUIET COMPONENTS Core5Compat)

if(NOT TARGET Qt6::Core5Compat)
    message(FATAL_ERROR
        "Scintilla 5.6.6's Qt 6 backend requires Qt6 Core5Compat. "
        "Install the Qt 5 Compatibility Module for Qt 6.11.2 MSVC 2022 64-bit "
        "with the Qt Maintenance Tool, then configure again."
    )
endif()

set(NTE_SCINTILLA_VERSION "5.6.6")
set(NTE_LEXILLA_VERSION "5.5.3")

set(_nte_scintilla_vendor "${CMAKE_SOURCE_DIR}/third_party/scintilla")
set(_nte_lexilla_vendor "${CMAKE_SOURCE_DIR}/third_party/lexilla")

if(EXISTS "${_nte_scintilla_vendor}/include/Scintilla.h")
    set(NTE_SCINTILLA_ROOT "${_nte_scintilla_vendor}")
else()
    FetchContent_Declare(
        nte_scintilla_source
        URL "https://www.scintilla.org/scintilla566.zip"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(nte_scintilla_source)

    if(EXISTS "${nte_scintilla_source_SOURCE_DIR}/scintilla/include/Scintilla.h")
        set(NTE_SCINTILLA_ROOT "${nte_scintilla_source_SOURCE_DIR}/scintilla")
    elseif(EXISTS "${nte_scintilla_source_SOURCE_DIR}/include/Scintilla.h")
        set(NTE_SCINTILLA_ROOT "${nte_scintilla_source_SOURCE_DIR}")
    else()
        message(FATAL_ERROR "Scintilla ${NTE_SCINTILLA_VERSION} source layout was not recognized.")
    endif()
endif()

if(EXISTS "${_nte_lexilla_vendor}/include/Lexilla.h")
    set(NTE_LEXILLA_ROOT "${_nte_lexilla_vendor}")
else()
    FetchContent_Declare(
        nte_lexilla_source
        URL "https://www.scintilla.org/lexilla553.zip"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(nte_lexilla_source)

    if(EXISTS "${nte_lexilla_source_SOURCE_DIR}/lexilla/include/Lexilla.h")
        set(NTE_LEXILLA_ROOT "${nte_lexilla_source_SOURCE_DIR}/lexilla")
    elseif(EXISTS "${nte_lexilla_source_SOURCE_DIR}/include/Lexilla.h")
        set(NTE_LEXILLA_ROOT "${nte_lexilla_source_SOURCE_DIR}")
    else()
        message(FATAL_ERROR "Lexilla ${NTE_LEXILLA_VERSION} source layout was not recognized.")
    endif()
endif()

set(_nte_scintilla_qt "${NTE_SCINTILLA_ROOT}/qt/ScintillaEditBase")
set(_nte_scintilla_src "${NTE_SCINTILLA_ROOT}/src")

add_library(
    nte_scintilla_qt
    STATIC
        "${_nte_scintilla_qt}/PlatQt.cpp"
        "${_nte_scintilla_qt}/PlatQt.h"
        "${_nte_scintilla_qt}/ScintillaEditBase.cpp"
        "${_nte_scintilla_qt}/ScintillaEditBase.h"
        "${_nte_scintilla_qt}/ScintillaQt.cpp"
        "${_nte_scintilla_qt}/ScintillaQt.h"
        "${_nte_scintilla_src}/AutoComplete.cxx"
        "${_nte_scintilla_src}/CallTip.cxx"
        "${_nte_scintilla_src}/CaseConvert.cxx"
        "${_nte_scintilla_src}/CaseFolder.cxx"
        "${_nte_scintilla_src}/CellBuffer.cxx"
        "${_nte_scintilla_src}/ChangeHistory.cxx"
        "${_nte_scintilla_src}/CharacterCategoryMap.cxx"
        "${_nte_scintilla_src}/CharacterType.cxx"
        "${_nte_scintilla_src}/CharClassify.cxx"
        "${_nte_scintilla_src}/ContractionState.cxx"
        "${_nte_scintilla_src}/DBCS.cxx"
        "${_nte_scintilla_src}/Decoration.cxx"
        "${_nte_scintilla_src}/Document.cxx"
        "${_nte_scintilla_src}/EditModel.cxx"
        "${_nte_scintilla_src}/Editor.cxx"
        "${_nte_scintilla_src}/EditView.cxx"
        "${_nte_scintilla_src}/Geometry.cxx"
        "${_nte_scintilla_src}/Indicator.cxx"
        "${_nte_scintilla_src}/KeyMap.cxx"
        "${_nte_scintilla_src}/LineMarker.cxx"
        "${_nte_scintilla_src}/MarginView.cxx"
        "${_nte_scintilla_src}/PerLine.cxx"
        "${_nte_scintilla_src}/PositionCache.cxx"
        "${_nte_scintilla_src}/RESearch.cxx"
        "${_nte_scintilla_src}/RunStyles.cxx"
        "${_nte_scintilla_src}/ScintillaBase.cxx"
        "${_nte_scintilla_src}/Selection.cxx"
        "${_nte_scintilla_src}/Style.cxx"
        "${_nte_scintilla_src}/UndoHistory.cxx"
        "${_nte_scintilla_src}/UniConversion.cxx"
        "${_nte_scintilla_src}/UniqueString.cxx"
        "${_nte_scintilla_src}/ViewStyle.cxx"
        "${_nte_scintilla_src}/XPM.cxx"
)

set_target_properties(
    nte_scintilla_qt
    PROPERTIES
        AUTOMOC ON
)

target_include_directories(
    nte_scintilla_qt
    PUBLIC
        "${NTE_SCINTILLA_ROOT}/include"
        "${NTE_SCINTILLA_ROOT}/src"
        "${_nte_scintilla_qt}"
)

target_compile_definitions(
    nte_scintilla_qt
    PUBLIC
        EXPORT_IMPORT_API=
    PRIVATE
        SCINTILLA_QT=1
)

target_link_libraries(
    nte_scintilla_qt
    PUBLIC
        Qt6::Widgets
        Qt6::Core5Compat
)

# Lexilla is Qt-free. Build the pinned source statically and expose its public
# protocol (CreateLexer) to the application. GLOB is intentionally contained
# inside this third-party adapter so upstream lexer additions remain complete.
file(
    GLOB
    _nte_lexilla_lexlib_sources
    CONFIGURE_DEPENDS
    "${NTE_LEXILLA_ROOT}/lexlib/*.cxx"
)

file(
    GLOB
    _nte_lexilla_lexer_sources
    CONFIGURE_DEPENDS
    "${NTE_LEXILLA_ROOT}/lexers/*.cxx"
)

add_library(
    nte_lexilla
    STATIC
        "${NTE_LEXILLA_ROOT}/src/Lexilla.cxx"
        ${_nte_lexilla_lexlib_sources}
        ${_nte_lexilla_lexer_sources}
)

target_include_directories(
    nte_lexilla
    PUBLIC
        "${NTE_LEXILLA_ROOT}/include"
        "${NTE_LEXILLA_ROOT}/lexlib"
        "${NTE_SCINTILLA_ROOT}/include"
)

target_compile_definitions(
    nte_lexilla
    PRIVATE
        LEXILLA_NO_EXPORT=1
)
