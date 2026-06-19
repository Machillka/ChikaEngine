if(NOT DEFINED PROJECT_ROOT_DIR)
    message(FATAL_ERROR "PROJECT_ROOT_DIR is required")
endif()

file(GLOB_RECURSE RUNTIME_BOUNDARY_FILES
    LIST_DIRECTORIES false
    "${PROJECT_ROOT_DIR}/engine/Runtime/*.h"
    "${PROJECT_ROOT_DIR}/engine/Runtime/*.hpp"
    "${PROJECT_ROOT_DIR}/engine/Runtime/*.cpp"
)

set(RUNTIME_FORBIDDEN_PATTERNS
    "ImGui"
    "Imgui"
    "imgui"
    "imgui_impl"
    "SubmitImGui"
    "GetImGui"
    "DrawImGui"
    "InitializeImgui"
    "EditorManager"
    "IEditorPanel"
)

foreach(file_path IN LISTS RUNTIME_BOUNDARY_FILES)
    file(READ "${file_path}" file_content)
    foreach(pattern IN LISTS RUNTIME_FORBIDDEN_PATTERNS)
        if(file_content MATCHES "${pattern}")
            message(FATAL_ERROR "Runtime boundary violation: ${file_path} contains ${pattern}")
        endif()
    endforeach()
endforeach()

file(GLOB_RECURSE GAME_BOUNDARY_FILES
    LIST_DIRECTORIES false
    "${PROJECT_ROOT_DIR}/engine/Game/*"
)

foreach(file_path IN LISTS GAME_BOUNDARY_FILES)
    file(READ "${file_path}" file_content)
    if(file_content MATCHES "ChikaEditor|EditorManager|ImGui|imgui")
        message(FATAL_ERROR "Standalone Game boundary violation: ${file_path} references Editor UI")
    endif()
endforeach()

message(STATUS "Standalone Runtime boundary check passed")
