cmake_minimum_required(VERSION 3.16)
project(BMPViewerQt VERSION 1.0 LANGUAGES CXX)

set(APP_EXE_NAME "MedicalImageAnalysisPlatform")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)
find_package(VTK REQUIRED COMPONENTS
    CommonCore
    CommonColor
    CommonDataModel
    CommonExecutionModel
    FiltersCore
    FiltersGeneral
    FiltersSources
    ImagingCore
    ImagingGeneral
    ImagingMorphological
    RenderingCore
    RenderingOpenGL2
)

add_executable(BMPViewerQt
    src/main.cpp
    src/BMPReader.cpp
    src/DicomToolkit.cpp
    src/DicomToolkit.h
    src/DicomSegLoader.cpp
    src/DicomSegLoader.h
    src/DicomProcessingPipeline.cpp
    src/DicomProcessingPipeline.h
    src/SliceNavigationController.cpp
    src/SliceNavigationController.h
    src/SegmentVolume.cpp
    src/SegmentVolume.h
    src/SegmentationOverlay.cpp
    src/SegmentationOverlay.h
    src/SegmentationMeshBuilder.cpp
    src/SegmentationMeshBuilder.h
    src/Segmentation3DView.cpp
    src/Segmentation3DView.h
    src/ImageLabProcessor.cpp
    src/ImageRestoration.cpp
    src/ImageRestoration.h
    src/FourierExperimentWidget.cpp
    src/FourierExperimentWidget.h
    src/MainWindow.cpp
    src/ImageView.cpp
    src/ImageHistTools.cpp
    src/ImageHistTools.h
)

target_include_directories(BMPViewerQt PRIVATE src)
target_link_libraries(BMPViewerQt PRIVATE
    Qt6::Widgets
    Qt6::Concurrent
    VTK::CommonCore
    VTK::CommonColor
    VTK::CommonDataModel
    VTK::CommonExecutionModel
    VTK::FiltersCore
    VTK::FiltersGeneral
    VTK::FiltersSources
    VTK::ImagingCore
    VTK::ImagingGeneral
    VTK::ImagingMorphological
    VTK::RenderingCore
    VTK::RenderingOpenGL2
)
vtk_module_autoinit(
    TARGETS BMPViewerQt
    MODULES ${VTK_LIBRARIES}
)
set_target_properties(BMPViewerQt PROPERTIES OUTPUT_NAME "${APP_EXE_NAME}")

if (WIN32)
    add_custom_command(TARGET BMPViewerQt POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy -t "$<TARGET_FILE_DIR:BMPViewerQt>" $<TARGET_RUNTIME_DLLS:BMPViewerQt>
        COMMAND_EXPAND_LISTS
        COMMENT "Copying target runtime DLLs"
    )
endif()

if (WIN32)
    set_target_properties(BMPViewerQt PROPERTIES WIN32_EXECUTABLE TRUE)

    get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

    if (WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET BMPViewerQt POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --release
                --no-compiler-runtime
                --no-translations
                --no-system-d3d-compiler
                "$<TARGET_FILE:BMPViewerQt>"
            COMMENT "Deploying Qt runtime with windeployqt"
        )
    endif()
endif()
