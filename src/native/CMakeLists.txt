cmake_minimum_required(VERSION 3.10)
project(native)

set(libmono_include_dirs 
	"${CMAKE_SOURCE_DIR}/vendor/mono-headers/include/mono-2.0"
)

add_library(libmono-2.0 SHARED IMPORTED)
set_target_properties(libmono-2.0 PROPERTIES
	IMPORTED_LOCATION ${coreclr_dll_file}
	IMPORTED_IMPLIB ${coreclr_lib_file}
	INTERFACE_INCLUDE_DIRECTORIES "${libmono_include_dirs}"
)

include(FetchContent)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz DOWNLOAD_EXTRACT_TIMESTAMP NEW)
FetchContent_MakeAvailable(json)

add_executable(main main.cpp)
target_link_libraries(main
		libmono-2.0
		nlohmann_json::nlohmann_json)
install(TARGETS main
	RUNTIME 
	DESTINATION ${LOCAL_INSTALL_PREFIX}/native)

if(MINGW)
	target_link_options(main PRIVATE "LINKER:--no-undefined")
endif()
