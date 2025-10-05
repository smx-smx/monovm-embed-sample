if(MSVC)
	get_filename_component(_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
	find_program(LIB_EXE
		NAMES lib
		HINTS ${_compiler_dir}
		REQUIRED)
	find_program(DUMPBIN_EXE
		NAMES dumpbin
		HINTS ${_compiler_dir}
		REQUIRED)
else()
	find_program(DLLTOOL_EXE dlltool REQUIRED)
endif()

add_custom_target(gen_runtime)

## step 1: obtain the runtime pack

set(DOTNET_ARGS
	-f ${OPT_DOTNET_FRAMEWORK}
	-r ${OPT_DOTNET_RID}
	-c ${CMAKE_BUILD_TYPE}
	#/p:BaseOutputPath=${LOCAL_INSTALL_PREFIX}/GetRuntimePack/bin/
	/p:BaseIntermediateOutputPath=${LOCAL_INSTALL_PREFIX}/GetRuntimePack/obj/
	/p:OutputPath=${LOCAL_INSTALL_PREFIX}/GetRuntimePack/
)
add_custom_target(GetRuntimePack ALL
	COMMAND ${DOTNET_EXE} publish
		${DOTNET_ARGS} --self-contained
		${CMAKE_CURRENT_SOURCE_DIR}/GetRuntimePack.csproj
)
add_dependencies(gen_runtime GetRuntimePack)

## step 2: read the runtime pack path from the .txt (produced by the build)
## then, copy native libs

set(csproj_outdir ${LOCAL_INSTALL_PREFIX}/GetRuntimePack)
set(runtime_pack_dir_file ${csproj_outdir}/runtime-pack-dir.txt)

if(MINGW)
	set(lib_prefix "")
else()
	set(lib_prefix ${CMAKE_SHARED_LIBRARY_PREFIX})
endif()

add_custom_command(
	OUTPUT ${LOCAL_INSTALL_PREFIX}/native/.touch-copylibs
	COMMAND ${CMAKE_COMMAND} 
		-D CMAKE_SHARED_LIBRARY_PREFIX=${lib_prefix}
		-D CMAKE_SHARED_LIBRARY_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}
		-D OPT_DOTNET_RID=${OPT_DOTNET_RID}
		-D RUNTIME_PACK_DIR_FILE=${runtime_pack_dir_file}
		-D DESTDIR=${LOCAL_INSTALL_PREFIX}/native
		-P ${CMAKE_CURRENT_SOURCE_DIR}/copy_runtime.cmake
)
add_custom_target(gen_runtime_libs
	DEPENDS ${LOCAL_INSTALL_PREFIX}/native/.touch-copylibs)
add_dependencies(gen_runtime_libs GetRuntimePack)
add_dependencies(gen_runtime gen_runtime_libs)

## step 3: generate coreclr import library
set(coreclr_prefix ${CMAKE_IMPORT_LIBRARY_PREFIX}coreclr)

if(WIN32)
	if(MINGW)
		set(coreclr_dll_file ${LOCAL_INSTALL_PREFIX}/native/coreclr${CMAKE_SHARED_LIBRARY_SUFFIX})
	else()
		set(coreclr_dll_file ${LOCAL_INSTALL_PREFIX}/native/${CMAKE_SHARED_LIBRARY_PREFIX}coreclr${CMAKE_SHARED_LIBRARY_SUFFIX})
	endif()
	
	set(coreclr_def_file ${LOCAL_INSTALL_PREFIX}/native/${coreclr_prefix}.def)
	set(coreclr_lib_file ${LOCAL_INSTALL_PREFIX}/native/${coreclr_prefix}${CMAKE_IMPORT_LIBRARY_SUFFIX})

	if(MSVC)
		add_custom_command(
			OUTPUT ${coreclr_def_file} ${coreclr_lib_file}
			COMMAND ${CMAKE_COMMAND}
				-D LIB_EXE=${LIB_EXE}
				-D DUMPBIN_EXE=${DUMPBIN_EXE}
				-D OPT_DOTNET_RID=${OPT_DOTNET_RID}
				-D IN_DLL=${coreclr_dll_file}
				-D OUT_DEF=${coreclr_def_file}
				-D OUT_LIB=${coreclr_lib_file}
				-P ${CMAKE_CURRENT_SOURCE_DIR}/gen_lib_msvc.cmake
		)
	else(MSVC)
		if(CMAKE_SIZEOF_VOID_P EQUAL 4)
			set(LIB_MACHINE i386)
		elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
			set(LIB_MACHINE i386:x86-64)
		else()
			message(FATAL_ERROR "Unknown pointer size")
		endif()
		# provided by
		# - mingw-w64-i686-tools
		# - mingw-w64-x86_64-tools
		find_program(GENDEF_EXE gendef REQUIRED)
		find_program(DLLTOOL_EXE dlltool REQUIRED)
		add_custom_command(
			OUTPUT ${coreclr_def_file} ${coreclr_lib_file}
			COMMAND ${CMAKE_COMMAND}
				-D GENDEF_EXE=${GENDEF_EXE}
				-D DLLTOOL_EXE=${DLLTOOL_EXE}
				-D OPT_DOTNET_RID=${OPT_DOTNET_RID}
				-D IN_DLL=${coreclr_dll_file}
				-D OUT_DEF=${coreclr_def_file}
				-D OUT_LIB=${coreclr_lib_file}
				-P ${CMAKE_CURRENT_SOURCE_DIR}/gen_lib_mingw.cmake
		)
	endif(MSVC)
	add_custom_target(gen_coreclr_implib DEPENDS ${coreclr_def_file} ${coreclr_lib_file})
	add_dependencies(gen_coreclr_implib gen_runtime_libs)

	add_dependencies(gen_runtime gen_coreclr_implib)
endif(WIN32)


