cmake_minimum_required(VERSION 3.10)
project(native)

## generate tpa.h
set(csproj_publish_dir ${LOCAL_INSTALL_PREFIX}/CSharpSample/bin/publish)

set(TPA_HEADER ${LOCAL_INSTALL_PREFIX}/native/generated/tpa.h)
add_custom_command(
	OUTPUT ${TPA_HEADER}
	COMMAND ${CMAKE_COMMAND}
		-D IN_PUBLISH_DIR=${csproj_publish_dir}
		-D OUT_HEADER=${TPA_HEADER}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/gen_managed.cmake
)
add_custom_target(gen_tpa_header DEPENDS ${TPA_HEADER})

if(MINGW)
	set(coreclr_dll_file ${LOCAL_INSTALL_PREFIX}/native/coreclr${CMAKE_SHARED_LIBRARY_SUFFIX})
else()
	set(coreclr_dll_file ${LOCAL_INSTALL_PREFIX}/native/${CMAKE_SHARED_LIBRARY_PREFIX}coreclr${CMAKE_SHARED_LIBRARY_SUFFIX})
endif()
set(coreclr_prefix ${CMAKE_IMPORT_LIBRARY_PREFIX}coreclr)
set(coreclr_lib_file ${LOCAL_INSTALL_PREFIX}/native/${coreclr_prefix}${CMAKE_IMPORT_LIBRARY_SUFFIX})

# to make IMPORTED_INCLUDE_DIRECTORIES happy at configure time
file(MAKE_DIRECTORY ${LOCAL_INSTALL_PREFIX}/native/generated)

set(libmono_include_dirs 
	"${CMAKE_SOURCE_DIR}/vendor/mono-headers/include/mono-2.0"
	"${LOCAL_INSTALL_PREFIX}/native/generated"
)
cmake_path (SET CORELIB_FULL_PATH "${LOCAL_INSTALL_PREFIX}/native/System.Private.CoreLib.dll")
cmake_path (ABSOLUTE_PATH CORELIB_FULL_PATH)

set(libmono_compile_defs "CORELIB_FULL_PATH=\"${CORELIB_FULL_PATH}\"")

add_library(libmono-2.0 SHARED IMPORTED)
set_target_properties(libmono-2.0 PROPERTIES
	IMPORTED_LOCATION ${coreclr_dll_file}
	IMPORTED_IMPLIB ${coreclr_lib_file}
	INTERFACE_INCLUDE_DIRECTORIES "${libmono_include_dirs}"
	INTERFACE_COMPILE_DEFINITIONS "${libmono_compile_defs}"
)

add_executable(main main.c)
target_link_libraries(main libmono-2.0)
install(TARGETS main
	RUNTIME 
	DESTINATION ${LOCAL_INSTALL_PREFIX}/native)

if(MINGW)
	target_link_options(main PRIVATE "LINKER:--no-undefined")
endif()
