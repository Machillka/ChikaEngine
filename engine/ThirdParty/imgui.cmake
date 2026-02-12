set(IMGUI_DIR "${CMAKE_CURRENT_LIST_DIR}/imgui")
if(NOT EXISTS "${IMGUI_DIR}")
    message(FATAL_ERROR "ImGui directory not found: ${IMGUI_DIR}")
endif()

# 可选配置（可在 include 之前通过 set(...) CACHE 覆盖）
if(NOT DEFINED IMGUI_BUILD_BACKENDS)
    set(IMGUI_BUILD_BACKENDS ON CACHE BOOL "Build ImGui backends (GLFW + OpenGL3)" FORCE)
endif()
if(NOT DEFINED IMGUI_GL_LOADER)
    set(IMGUI_GL_LOADER "glad" CACHE STRING "GL loader to use for imgui_impl_opengl3 (glad|none)" FORCE)
endif()
if(NOT DEFINED IMGUI_GLSL_VERSION)
    set(IMGUI_GLSL_VERSION "#version 130" CACHE STRING "GLSL version string for ImGui_ImplOpenGL3_Init" FORCE)
endif()

# 2) 收集源文件（最简单：glob）
file(GLOB imgui_core_sources CONFIGURE_DEPENDS
    "${IMGUI_DIR}/*.cpp"
)

set(imgui_backend_sources "")
if(IMGUI_BUILD_BACKENDS)
    list(APPEND imgui_backend_sources
        "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
        "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
    )
endif()

set(IMGUI_ALL_SOURCES ${imgui_core_sources} ${imgui_backend_sources})

# 3) 创建静态库目标
add_library(imgui STATIC ${IMGUI_ALL_SOURCES})

# 公开 include 路径（核心 + backends）
target_include_directories(imgui PUBLIC
    $<BUILD_INTERFACE:${IMGUI_DIR}>
    $<BUILD_INTERFACE:${IMGUI_DIR}/backends>
)

# 4) 设置编译选项与宏
target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS=1)

# 5) 链接依赖（因为我们在 third_party 顶层已经创建了 glfw、glad、并 find_package(OpenGL)）
if(IMGUI_BUILD_BACKENDS)
    # 期望 glfw 目标由 third_party/CMakeLists 已经 add_subdirectory(glfw) 创建
    # 期望 glad 目标由 third_party/CMakeLists 已经创建
    # OpenGL::GL 由 find_package(OpenGL) 提供
    target_link_libraries(imgui PUBLIC glfw glad OpenGL::GL)

    # 定义 loader 宏（如果使用 glad）
    if(IMGUI_GL_LOADER STREQUAL "glad")
        target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD)
    elseif(IMGUI_GL_LOADER STREQUAL "none")
        # 不定义 loader 宏，使用者需自行在最终可执行目标处定义并链接 loader
    else()
        message(WARNING "IMGUI_GL_LOADER unknown: ${IMGUI_GL_LOADER}. Use 'glad' or 'none'. Defaulting to 'glad'.")
        target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD)
    endif()
endif()

# 6) 暴露别名并保存 GLSL 版本为目标属性（便于上层查询）
add_library(imgui::imgui ALIAS imgui)
set_target_properties(imgui PROPERTIES IMGUI_GLSL_VERSION "${IMGUI_GLSL_VERSION}")

message(STATUS "imgui: target created (backends: ${IMGUI_BUILD_BACKENDS}, loader: ${IMGUI_GL_LOADER}, glsl: ${IMGUI_GLSL_VERSION})")
# Written by Copilot