cmake_minimum_required(VERSION 3.10)
project(monovm-embed-sample LANGUAGES C CXX)

option(OPT_DOTNET_RID "Runtime Identifier")
if(NOT OPT_DOTNET_RID)
	message(FATAL_ERROR "OPT_DOTNET_RID required")
endif()

if(NOT DEFINED CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
	set(CMAKE_BUILD_TYPE Release)
endif()

set(LOCAL_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/out)

SET(OPT_DOTNET_FRAMEWORK "net8.0")

find_program(DOTNET_EXE dotnet REQUIRED)
add_subdirectory(src)
