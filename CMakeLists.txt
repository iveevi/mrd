cmake_minimum_required(VERSION 4.0)

project(mrd CXX)

set(MRD_GLM_PATH_RELATIVE "${MRD_GLM_PATH}")
cmake_path(RELATIVE_PATH MRD_GLM_PATH_RELATIVE
	BASE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")

message(STATUS "MRD GLM path provided ${MRD_GLM_PATH}")
message(STATUS "MRD GLM relative path ${MRD_GLM_PATH_RELATIVE}")
include_directories("${MRD_GLM_PATH_RELATIVE}")

if (NOT DEFINED MRD_STB_PATH)
	message(FATAL_ERROR "MRD_STB_PATH must point to the directory containing stb headers")
endif()

set(MRD_STB_PATH_RELATIVE "${MRD_STB_PATH}")
cmake_path(RELATIVE_PATH MRD_STB_PATH_RELATIVE
	BASE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")

message(STATUS "MRD stb path provided ${MRD_STB_PATH}")
message(STATUS "MRD stb relative path ${MRD_STB_PATH_RELATIVE}")
include_directories("${MRD_STB_PATH_RELATIVE}")

add_library(mrd
	source/model.cpp
	source/texture.cpp
)

target_include_directories(mrd PUBLIC
	${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(mrd assimp)
