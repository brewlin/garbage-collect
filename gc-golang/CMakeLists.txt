if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)
cmake_minimum_required(VERSION 3.10)

project(gc)

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -w")

include_directories(. atomic cache central mgc heap malloc span proc sys)

add_subdirectory(atomic)
add_subdirectory(cache)
add_subdirectory(central)
add_subdirectory(mgc)
add_subdirectory(heap)
add_subdirectory(malloc)
add_subdirectory(span)
add_subdirectory(proc)
add_subdirectory(sys)

add_executable(gc test.c)

target_link_libraries(gc atomic span cache atomic central span mgc heap malloc mgc span proc sys -lpthread)

