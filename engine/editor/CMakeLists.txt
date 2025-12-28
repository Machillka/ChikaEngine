set(TARGET_NAME ChikaEditor)
add_executable(${TARGET_NAME} main.cpp)
target_link_libraries(${TARGET_NAME} PRIVATE EngineCore)