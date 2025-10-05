function(read_file_line file_path out_var)
	file(READ ${file_path} contents)
	string(REGEX MATCH "^[^\r\n]*" first_line "${contents}")
	set(${out_var} ${first_line} PARENT_SCOPE)
endfunction()

read_file_line(${RUNTIME_PACK_DIR_FILE} runtime_dir_path)
string(REPLACE "\\" "/" runtime_dir_path "${runtime_dir_path}")

message(STATUS "Runtime Pack Path: ${runtime_dir_path}")
set(runtime_native_path ${runtime_dir_path}/runtimes/${OPT_DOTNET_RID}/native)

set(glob_expr "${runtime_native_path}/${CMAKE_SHARED_LIBRARY_PREFIX}*${CMAKE_SHARED_LIBRARY_SUFFIX}")
message(STATUS "Searching for libraries: ${glob_expr}")

file(GLOB DLL_FILES "${glob_expr}")
file(COPY ${DLL_FILES} DESTINATION ${DESTDIR})
file(COPY ${runtime_native_path}/System.Private.CoreLib.dll DESTINATION ${DESTDIR})
file(TOUCH ${DESTDIR}/.touch-copylibs)