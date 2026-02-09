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


set(REGISTRY_CPP "${PROJECT_BINARY_DIR}/generated/ReflectionRegistry.cpp")
set(REGISTRY_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/tools/reflector/register.py")
set(REGISTRY_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/tools/reflector/templates/register.cpp.j2")
get_property(ALL_REFLECT_HEADERS GLOBAL PROPERTY CHIKA_REFLECT_HEADERS)

# 3. 添加生成 Registry.cpp 的规则
add_custom_command(
    OUTPUT "${REGISTRY_CPP}"
    COMMAND ${UV_EXE} run ${REGISTRY_SCRIPT}
            --engine-root "${PROJECT_ROOT_DIR}"
            --output "${REGISTRY_CPP}"
            --template "${REGISTRY_TEMPLATE}"
            --headers ${ALL_REFLECT_HEADERS}
    WORKING_DIRECTORY "${PROJECT_ROOT_DIR}/engine/tools/reflector"
    # 只要头文件列表变了 或 脚本 / 模板变了，就重新生成
    DEPENDS ${ALL_REFLECT_HEADERS} "${REGISTRY_SCRIPT}" "${REGISTRY_TEMPLATE}"
    COMMENT "Reflection: Generating Registry (Linking ${ALL_REFLECT_HEADERS} units)"
    VERBATIM
)

set(ENGINE_SRC
	${CMAKE_CURRENT_LIST_DIR}/engine.cpp
)
set(ENGINE_HDR
	${CMAKE_CURRENT_LIST_DIR}/engine.h
)

add_library(ChikaEngine STATIC ${ENGINE_SRC} ${ENGINE_HDR} ${REGISTRY_CPP})

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
		ChikaPhysics
)
set_target_properties(ChikaEngine PROPERTIES
	CXX_STANDARD 20
	CXX_STANDARD_REQUIRED ON
)