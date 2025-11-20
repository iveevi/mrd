cmake_minimum_required(VERSION 4.0)

project(mrd CXX)

set(MRD_GLM_PATH_RELATIVE "${MRD_GLM_PATH}")
cmake_path(RELATIVE_PATH MRD_GLM_PATH_RELATIVE
	BASE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")

message(STATUS "MRD GLM path provided ${MRD_GLM_PATH}")
message(STATUS "MRD GLM relative path ${MRD_GLM_PATH_RELATIVE}")
include_directories("${MRD_GLM_PATH_RELATIVE}")

add_library(mrd mrd.cpp)

target_link_libraries(mrd assimp)
