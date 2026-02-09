# 定义一个全局属性，用来在不同目录间传递头文件列表
define_property(GLOBAL PROPERTY CHIKA_REFLECT_HEADERS
    BRIEF_DOCS "All reflection headers"
    FULL_DOCS "List of all header files that need reflection registration"
)
# 把 system include 先写到文件去 方便 debug
set(SYSTEM_INCLUDES_FILE "${CMAKE_BINARY_DIR}/system_includes.txt")
file(GENERATE OUTPUT ${SYSTEM_INCLUDES_FILE} CONTENT "${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES}")

# 判断是否是 MSVC ( 后期改成直接传入参数 )
if(MSVC)
    file(APPEND "${CMAKE_BINARY_DIR}/compiler_info.txt" "MSVC")
else()
    file(APPEND "${CMAKE_BINARY_DIR}/compiler_info.txt" "CLANG_OR_GCC")
endif()

function(reflection_generator TARGET_NAME)
    # 获取目标的所有源文件
    get_target_property(SRCS ${TARGET_NAME} SOURCES)
    if(NOT SRCS)
        return()
    endif()

    set(GEN_DIR "${PROJECT_BINARY_DIR}/generated/${TARGET_NAME}")
    file(MAKE_DIRECTORY "${GEN_DIR}")
    
    set(TARGET_GEN_FILES "")

    foreach(FILE ${SRCS})
        # 统一转为绝对路径
        if(NOT IS_ABSOLUTE "${FILE}")
            get_filename_component(FILE_PATH "${FILE}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        else()
            set(FILE_PATH "${FILE}")
        endif()

        if(EXISTS "${FILE_PATH}")
            # 简单预检：只处理包含 MCLASS 宏的文件
            file(READ "${FILE_PATH}" CONTENTS)
            if(CONTENTS MATCHES "MCLASS" AND NOT CONTENTS MATCHES "#define MCLASS")
                
                # 计算输出路径 (保持目录结构)
                file(RELATIVE_PATH REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}" "${FILE_PATH}")
                get_filename_component(REL_DIR "${REL_PATH}" DIRECTORY)
                get_filename_component(NAME_WE "${FILE_PATH}" NAME_WE)
                
                set(OUT_GEN_CPP "${GEN_DIR}/${REL_DIR}/${NAME_WE}.gen.cpp")

                # 添加生成命令 (调用 parser.py)
                add_custom_command(
                    OUTPUT "${OUT_GEN_CPP}"
                    COMMAND ${UV_EXE} run --project "${PROJECT_ROOT_DIR}/engine/tools/reflector"
                            parser.py
                            --input "${FILE_PATH}"
                            --output "${OUT_GEN_CPP}"
                            --build-dir "${PROJECT_BINARY_DIR}"
                            --engine-root "${PROJECT_ROOT_DIR}"
                    WORKING_DIRECTORY "${PROJECT_ROOT_DIR}/engine/tools/reflector"
                    DEPENDS "${FILE_PATH}"
                    COMMENT "Reflection: Parsing ${NAME_WE}.h"
                    VERBATIM
                )

                list(APPEND TARGET_GEN_FILES "${OUT_GEN_CPP}")
                message(STATUS "Generated reflection file: ${OUT_GEN_CPP}")
                # 将该头文件添加到全局列表，供最后生成 Registry 使用
                set_property(GLOBAL APPEND PROPERTY CHIKA_REFLECT_HEADERS "${FILE_PATH}")
                
            endif()
        endif()
    endforeach()

    # 如果有生成的反射代码，加入到当前库的构建中
    if(TARGET_GEN_FILES)
        target_sources(${TARGET_NAME} PRIVATE ${TARGET_GEN_FILES})
        target_include_directories(${TARGET_NAME} PUBLIC "${GEN_DIR}")
    endif()

endfunction()
