cmake_minimum_required(VERSION 3.28)

project(
    NativeTeXEditor
    VERSION 0.4.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    if(NOT MSVC)
        message(FATAL_ERROR "Windows development requires the Microsoft MSVC v143 x64 toolset.")
    endif()

    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "Windows development requires an x64 compiler environment.")
    endif()

    if(NOT MSVC_VERSION EQUAL 1944)
        message(FATAL_ERROR
            "The verified Windows toolchain is MSVC 19.44.x (v143). "
            "Initialize an x64 v143 developer shell, then configure in a clean build directory."
        )
    endif()
endif()

include(cmake/Dependencies.cmake)

qt_standard_project_setup()

qt_add_executable(
    NativeTeXEditor
    src/main.cpp
    src/app/ApplicationPaths.cpp
    src/app/ApplicationPaths.h
    src/build/BuildService.cpp
    src/build/BuildService.h
    src/build/BuildTypes.cpp
    src/build/BuildTypes.h
    src/document/DocumentSession.cpp
    src/document/DocumentSession.h
    src/editor/IEditorBackend.h
    src/editor/ScintillaEditorWidget.cpp
    src/editor/ScintillaEditorWidget.h
    src/infrastructure/EnvironmentService.cpp
    src/infrastructure/EnvironmentService.h
    src/infrastructure/Logging.cpp
    src/infrastructure/Logging.h
    src/infrastructure/QtDocumentFileService.cpp
    src/infrastructure/QtDocumentFileService.h
    src/infrastructure/QtProcessSupervisor.cpp
    src/infrastructure/QtProcessSupervisor.h
    src/infrastructure/QtSettingsService.cpp
    src/infrastructure/QtSettingsService.h
    src/pdf/PdfPageLayoutMapper.cpp
    src/pdf/PdfPageLayoutMapper.h
    src/pdf/PdfTypes.h
    src/pdf/QtPdfBackend.cpp
    src/pdf/QtPdfBackend.h
    src/preview/PreviewService.cpp
    src/preview/PreviewService.h
    src/preview/PreviewTypes.h
    src/project/ProjectService.cpp
    src/project/ProjectService.h
    src/recovery/RecoveryService.cpp
    src/recovery/RecoveryService.h
    src/services/IBuildService.h
    src/services/IDocumentFileService.h
    src/services/IEnvironmentService.h
    src/services/IProcessSupervisor.h
    src/services/IProjectService.h
    src/services/IPdfBackend.h
    src/services/IPreviewService.h
    src/services/IRecoveryService.h
    src/services/ISettingsService.h
    src/services/ISyncTeXService.h
    src/synctex/SyncTeXService.cpp
    src/synctex/SyncTeXService.h
    src/synctex/SyncTeXTypes.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/RecoveryComparisonDialog.cpp
    src/ui/RecoveryComparisonDialog.h
)

target_include_directories(
    NativeTeXEditor
    PRIVATE
        src
)

target_link_libraries(
    NativeTeXEditor
    PRIVATE
        Qt6::Widgets
        Qt6::Pdf
        Qt6::PdfWidgets
        nte_scintilla_qt
        nte_lexilla
)

if(MSVC)
    target_compile_options(
        NativeTeXEditor
        PRIVATE
            /W4
            /permissive-
            /Zc:__cplusplus
            /utf-8
    )
endif()
