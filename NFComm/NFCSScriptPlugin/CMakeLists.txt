set(ProjectName "NFCSScriptPlugin")
file(GLOB NFCSScriptPlugin_ROOT_Cpp *.cpp)
file(GLOB NFCSScriptPlugin_ROOT_Hpp *.h)

add_library(NFCSScriptPlugin STATIC
	${NFCSScriptPlugin_ROOT_Cpp}
	${NFCSScriptPlugin_ROOT_Hpp})

if(UNIX)
	add_dependencies(NFCSScriptPlugin NFCore)
	
	if (CMAKE_BUILD_TYPE MATCHES "Release")
	target_link_libraries(NFCSScriptPlugin libprotobuf.a)
	else()
	target_link_libraries(NFCSScriptPlugin libprotobuf.a)
	endif()
	
	target_link_libraries(NFCSScriptPlugin -static-libstdc++)
else()
	add_dependencies(NFCSScriptPlugin NFCore )
	target_link_libraries(NFCSScriptPlugin libprotobuf )

	target_link_libraries(NFCSScriptPlugin -static-libstdc++)

endif()

set_target_properties( NFCSScriptPlugin PROPERTIES OUTPUT_NAME_DEBUG "NFCSScriptPlugin_d")
set_target_properties( NFCSScriptPlugin PROPERTIES PREFIX "")
set_target_properties( NFCSScriptPlugin PROPERTIES
	FOLDER "NFSDK"
	ARCHIVE_OUTPUT_DIRECTORY ${NFOutPutDir}
	RUNTIME_OUTPUT_DIRECTORY ${NFOutPutDir}
	LIBRARY_OUTPUT_DIRECTORY ${NFOutPutDir} )