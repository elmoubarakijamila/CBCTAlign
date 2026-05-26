cmake_minimum_required(VERSION 3.16)
project(CBCTAlign VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

option(BUILD_TESTS "Build unit tests" OFF)

# Qt (5 ou 6)
find_package(Qt6 COMPONENTS Widgets Core Gui OpenGLWidgets QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 COMPONENTS Widgets Core Gui OpenGL REQUIRED)
    set(QT_LIBRARIES Qt5::Widgets Qt5::Core Qt5::Gui Qt5::OpenGL)
    message(STATUS "Using Qt5")
else()
    set(QT_LIBRARIES Qt6::Widgets Qt6::Core Qt6::Gui Qt6::OpenGLWidgets)
    message(STATUS "Using Qt6")
endif()

find_package(ITK REQUIRED)
include(${ITK_USE_FILE})

find_package(VTK REQUIRED)
include(${VTK_USE_FILE})

find_package(Eigen3 REQUIRED)

# Sources Core (VolumePreprocessor supprimé — fonctionnalité intégrée dans CBCTVolume)
set(CORE_SOURCES
    src/core/CBCTVolume.cpp
    src/core/RigidRegistration.cpp
    src/core/CephalometricDetector.cpp
    src/core/SlicePlaneCalculator.cpp
    src/core/SliceExtractor.cpp
    src/core/ValidationMetrics.cpp
    src/core/PipelineManager.cpp
    src/core/LandmarkIO.cpp
    src/core/SliceNormalizer.cpp
)

set(CORE_HEADERS
    src/core/CBCTVolume.h
    src/core/RigidRegistration.h
    src/core/CephalometricDetector.h
    src/core/SlicePlaneCalculator.h
    src/core/SliceExtractor.h
    src/core/ValidationMetrics.h
    src/core/PipelineManager.h
    src/core/LandmarkIO.h
    src/core/SliceNormalizer.h
)

# Sources GUI
set(GUI_SOURCES
    src/gui/MainWindow.cpp
    src/gui/VolumeViewer3D.cpp
    src/gui/SliceViewer2D.cpp
    src/gui/LandmarkEditor.cpp
    src/gui/ComparisonWidget.cpp
    src/gui/ProgressDialog.cpp
    src/gui/LandmarkAnnotationWidget.cpp
)

set(GUI_HEADERS
    src/gui/MainWindow.h
    src/gui/VolumeViewer3D.h
    src/gui/SliceViewer2D.h
    src/gui/LandmarkEditor.h
    src/gui/ComparisonWidget.h
    src/gui/ProgressDialog.h
    src/gui/LandmarkAnnotationWidget.h
)

# Sources Utils
set(UTILS_SOURCES
    src/utils/MathUtils.cpp
    src/utils/FileIO.cpp
    src/utils/Logger.cpp
)

set(UTILS_HEADERS
    src/utils/MathUtils.h
    src/utils/FileIO.h
    src/utils/Logger.h
)

add_executable(CBCTAlign
    src/main.cpp
    ${CORE_SOURCES} ${CORE_HEADERS}
    ${GUI_SOURCES}  ${GUI_HEADERS}
    ${UTILS_SOURCES} ${UTILS_HEADERS}
)

target_include_directories(CBCTAlign PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
    ${EIGEN3_INCLUDE_DIR}
)

target_link_libraries(CBCTAlign
    ${QT_LIBRARIES}
    ${ITK_LIBRARIES}
    ${VTK_LIBRARIES}
    Eigen3::Eigen
)

install(TARGETS CBCTAlign DESTINATION bin)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "")
message(STATUS "=== CBCTAlign Configuration ===")
if(Qt6_FOUND)
    message(STATUS "Qt: ${Qt6_VERSION}")
else()
    message(STATUS "Qt: ${Qt5_VERSION}")
endif()
message(STATUS "ITK: ${ITK_VERSION}")
message(STATUS "VTK: ${VTK_VERSION}")
message(STATUS "Build: ${CMAKE_BUILD_TYPE}")
message(STATUS "===============================")
