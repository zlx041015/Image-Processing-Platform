cmake_minimum_required(VERSION 3.16)
project(BMPViewerQt VERSION 1.0 LANGUAGES CXX)

set(APP_EXE_NAME "MedicalImageAnalysisPlatform")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(BMPViewerQt
    src/main.cpp
    src/BMPReader.cpp
    src/ImageLabProcessor.cpp
    src/ImageRestoration.cpp
    src/ImageRestoration.h
    src/FourierExperimentWidget.cpp
    src/FourierExperimentWidget.h
    src/MainWindow.cpp
    src/MainWindow.ui
    src/ImageView.cpp
    src/ImageHistTools.cpp
    src/ImageHistTools.h
)

target_include_directories(BMPViewerQt PRIVATE src)
target_link_libraries(BMPViewerQt PRIVATE Qt6::Widgets)
set_target_properties(BMPViewerQt PROPERTIES OUTPUT_NAME "${APP_EXE_NAME}")

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
