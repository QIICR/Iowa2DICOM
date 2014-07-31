project(Iowa2DICOM)
cmake_minimum_required(VERSION 2.8)

set(LOCAL_SEM_NO_INSTALL TRUE)

include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/FindGit.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/MacroExtractRepositoryInfo.cmake)

SlicerMacroExtractRepositoryInfo(VAR_PREFIX Iowa2DICOM)
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/CMake/Iowa2DICOMVersionConfigure.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/Iowa2DICOMVersionConfigure.h
)

find_package(SlicerExecutionModel NO_MODULE REQUIRED GenerateCLP)
include(${SlicerExecutionModel_USE_FILE})
include(${SlicerExecutionModel_CMAKE_DIR}/SEMMacroBuildCLI.cmake)
include(${SlicerExecutionModel_USE_FILE})
include(${CMAKE_CURRENT_BINARY_DIR})

find_package(DCMTK REQUIRED)
find_package(ITK REQUIRED)

include(${ITK_USE_FILE})

include_directories(${DCMTK_INCLUDE_DIRS})
include_directories(${ITK_INCLUDE_DIRS})

add_subdirectory(ConvertSegmentations)
add_subdirectory(SingleTimepoint)
