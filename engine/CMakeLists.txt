set(ENGINE_ROOT_DIR "${PROJECT_ROOT_DIR}/engine")

add_subdirectory(core)
add_subdirectory(editor)    # editor 启动位置
add_subdirectory(framework)
add_subdirectory(platform)
add_subdirectory(render)
add_subdirectory(third_party)
add_subdirectory(input)
add_subdirectory(time)
add_subdirectory(resource_importer)
add_subdirectory(physics)

set(ENGINE_SRC
	${CMAKE_CURRENT_LIST_DIR}/engine.cpp
)
set(ENGINE_HDR
	${CMAKE_CURRENT_LIST_DIR}/engine.h
)

add_library(ChikaEngine STATIC ${ENGINE_SRC} ${ENGINE_HDR})

target_include_directories(ChikaEngine
	PUBLIC
		${CMAKE_CURRENT_LIST_DIR}    # allow including <engine.h>
)

target_link_libraries(ChikaEngine
	PUBLIC
		ChikaCore
		ChikaFramework
		ChikaRender
		ChikaPlatform
		ChikaInput
		ChikaTime
		ChikaThirdParty
		ChikaResource
)

set_target_properties(ChikaEngine PROPERTIES
	CXX_STANDARD 20
	CXX_STANDARD_REQUIRED ON
)