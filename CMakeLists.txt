cmake_minimum_required(VERSION 3.8)
file(READ "version" ver)
string(REGEX MATCH "VERSION_MAJOR ([0-9]*)" _ ${ver})
set(ver_major ${CMAKE_MATCH_1})
string(REGEX MATCH "VERSION_MINOR ([0-9]*)" _ ${ver})
set(ver_minor ${CMAKE_MATCH_1})
string(REGEX MATCH "VERSION_PATCH ([0-9]*)" _ ${ver})
set(ver_patch ${CMAKE_MATCH_1})
message("version: ${ver_major}.${ver_minor}.${ver_patch}")
project(zjson VERSION ${ver_major}.${ver_minor}.${ver_patch})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_BUILD_TYPE Debug)  # Use `cmake -DCMAKE_BUILD_TYPE=Debug` more better
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if(MSVC)
	string(APPEND CMAKE_CXX_FLAGS " /Zc:__cplusplus")
	add_compile_options(/wd26451 /wd26812 /wd4305 /wd4244)

    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
elseif(UNIX)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")

    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")
endif()

add_definitions(-DDEBUG)

FILE(GLOB_RECURSE SRC_FILES "./src/*.cpp") 
FILE(GLOB_RECURSE HEAD_FILES "./src/*.hpp") 

source_group("Head Files" FILES ${HEAD_FILES} )

add_executable(${PROJECT_NAME} ${SRC_FILES} ${HEAD_FILES} )

##--------------------- Build unit tests -----------------------------------##
option(CMAKE_ZJSON_ENABLE_TEST "Whether to enable unit tests" ON)
if (CMAKE_ZJSON_ENABLE_TEST)
    message(STATUS "Unit tests enabled")
    enable_testing()
    add_subdirectory(thirds/googletest-1.14.0 EXCLUDE_FROM_ALL)
    include_directories(thirds/googletest-1.14.0/googletest/include)

    add_executable(test_test tests/test_test.cpp)
    target_link_libraries(test_test gtest gtest_main)
    add_test(NAME test_test COMMAND test_test)

    add_executable(test_object tests/test_object.cpp)
    target_link_libraries(test_object gtest gtest_main)
    add_test(NAME test_object COMMAND test_object)

    add_executable(test_array tests/test_array.cpp)
    target_link_libraries(test_array gtest gtest_main)
    add_test(NAME test_array COMMAND test_array)

    add_executable(test_parse tests/test_parse.cpp)
    target_link_libraries(test_parse gtest gtest_main)
    add_test(NAME test_parse COMMAND test_parse)

endif()