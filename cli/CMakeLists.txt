cmake_minimum_required(VERSION 3.21)
project(aquila C)
enable_language(RC)

file(GLOB SOURCES ${PROJECT_SOURCE_DIR}/src/*.c)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(RESOURCE_FILE resources.rc)

add_executable(aquila ${SOURCES} ${RESOURCE_FILE})
target_include_directories(aquila PUBLIC ${PROJECT_SOURCE_DIR}/include)
set(CMAKE_C_FLAGS "-DWINDOWS_BUILD=1")