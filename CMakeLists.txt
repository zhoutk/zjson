cmake_minimum_required(VERSION 3.18)
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

option(CMAKE_ZJSON_ENABLE_ASAN "Enable AddressSanitizer for memory bug detection" OFF)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if(MSVC)
	string(APPEND CMAKE_CXX_FLAGS " /Zc:__cplusplus")
	add_compile_options(/wd26451 /wd26812 /wd4305 /wd4244)
elseif(UNIX)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")

    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")
endif()

if(CMAKE_ZJSON_ENABLE_ASAN)
    if(MSVC)
        message(WARNING "CMAKE_ZJSON_ENABLE_ASAN is ON, but this project currently enables ASan only for non-MSVC toolchains.")
    else()
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
        message(STATUS "AddressSanitizer enabled")
    endif()
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

    add_executable(test_more tests/test_more.cpp)
    target_link_libraries(test_more gtest gtest_main)
    add_test(NAME test_more COMMAND test_more)

    add_executable(test_spec tests/test_spec.cpp)
    target_link_libraries(test_spec gtest gtest_main)
    add_test(NAME test_spec COMMAND test_spec)

    add_executable(test_conformance tests/test_conformance.cpp)
    target_link_libraries(test_conformance gtest gtest_main)
    add_test(NAME test_conformance COMMAND test_conformance)

    add_executable(test_rfc_cases tests/test_rfc_cases.cpp)
    target_link_libraries(test_rfc_cases gtest gtest_main)
    add_test(NAME test_rfc_cases COMMAND test_rfc_cases)

    add_executable(test_jsontestsuite tests/test_jsontestsuite.cpp)
    target_link_libraries(test_jsontestsuite gtest gtest_main)
    add_test(NAME test_jsontestsuite COMMAND test_jsontestsuite)

endif()

##--------------------- Benchmark -------------------------------------------##
option(CMAKE_ZJSON_ENABLE_BENCH "Whether to build benchmark" ON)
if (CMAKE_ZJSON_ENABLE_BENCH)
    add_executable(bench_zjson bench/bench_main.cpp)
    if(MSVC)
        target_compile_options(bench_zjson PRIVATE $<$<CONFIG:Release>:/O2>)
    else()
        target_compile_options(bench_zjson PRIVATE $<$<CONFIG:Release>:-O2>)
    endif()
endif()