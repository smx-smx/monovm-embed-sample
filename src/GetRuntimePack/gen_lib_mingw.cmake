get_filename_component(DLL_NAME ${IN_DLL} NAME)

if(OPT_DOTNET_RID STREQUAL "win-x86")
	set(MACHINE "i386")
elseif(OPT_DOTNET_RID STREQUAL "win-x64")
	set(MACHINE "i386:x86_64")
else()
	message(FATAL_ERROR "Unknown RID")
endif()

execute_process(
	COMMAND ${GENDEF_EXE} - ${IN_DLL}
	OUTPUT_FILE ${OUT_DEF}
	COMMAND_ERROR_IS_FATAL ANY)

execute_process(
	COMMAND ${DLLTOOL_EXE} -d ${OUT_DEF} -D ${DLL_NAME} -l ${OUT_LIB} -m ${MACHINE}
	COMMAND_ERROR_IS_FATAL ANY)