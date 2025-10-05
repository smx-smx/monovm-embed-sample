set(DOTNET_ARGS
	-f ${OPT_DOTNET_FRAMEWORK}
	-r ${OPT_DOTNET_RID}
	-c ${CMAKE_BUILD_TYPE}
	#/p:BaseOutputPath=${LOCAL_INSTALL_PREFIX}/CSharpSample/bin/
	/p:BaseIntermediateOutputPath=${LOCAL_INSTALL_PREFIX}/CSharpSample/obj/
	/p:OutputPath=${LOCAL_INSTALL_PREFIX}/CSharpSample/bin/
)
add_custom_target(CSharpSample ALL
	COMMAND ${DOTNET_EXE} publish --self-contained
		${DOTNET_ARGS} ${CMAKE_CURRENT_SOURCE_DIR}/CSharpSample.csproj
)
add_dependencies(CSharpSample GetRuntimePack)