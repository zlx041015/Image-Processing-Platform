cmake_minimum_required(VERSION 3.16)
project(BMPViewerQt VERSION 1.0 LANGUAGES CXX)

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

if (WIN32)
    set_target_properties(BMPViewerQt PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
