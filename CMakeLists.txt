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

if(MSVC AND (NOT CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES OR CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES STREQUAL ""))
    get_filename_component(_zjson_msvc_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_zjson_msvc_host_dir "${_zjson_msvc_bin_dir}" DIRECTORY)
    get_filename_component(_zjson_msvc_bin_root "${_zjson_msvc_host_dir}" DIRECTORY)
    get_filename_component(_zjson_msvc_root "${_zjson_msvc_bin_root}" DIRECTORY)

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_zjson_msvc_arch x64)
    else()
        set(_zjson_msvc_arch x86)
    endif()

    set(_zjson_msvc_include_dir "${_zjson_msvc_root}/include")
    set(_zjson_msvc_lib_dir "${_zjson_msvc_root}/lib/${_zjson_msvc_arch}")
    set(_zjson_winsdk_root "C:/Program Files (x86)/Windows Kits/10")
    file(GLOB _zjson_winsdk_versions LIST_DIRECTORIES true "${_zjson_winsdk_root}/Include/10.*")
    list(SORT _zjson_winsdk_versions)
    list(REVERSE _zjson_winsdk_versions)
    list(GET _zjson_winsdk_versions 0 _zjson_winsdk_include_root)
    get_filename_component(_zjson_winsdk_version "${_zjson_winsdk_include_root}" NAME)

    set(_zjson_fallback_include_dirs
        "${_zjson_msvc_include_dir}"
        "${_zjson_winsdk_root}/Include/${_zjson_winsdk_version}/ucrt"
        "${_zjson_winsdk_root}/Include/${_zjson_winsdk_version}/shared"
        "${_zjson_winsdk_root}/Include/${_zjson_winsdk_version}/um"
        "${_zjson_winsdk_root}/Include/${_zjson_winsdk_version}/winrt"
        "${_zjson_winsdk_root}/Include/${_zjson_winsdk_version}/cppwinrt")
    set(_zjson_fallback_lib_dirs
        "${_zjson_msvc_lib_dir}"
        "${_zjson_winsdk_root}/Lib/${_zjson_winsdk_version}/ucrt/${_zjson_msvc_arch}"
        "${_zjson_winsdk_root}/Lib/${_zjson_winsdk_version}/um/${_zjson_msvc_arch}")

    foreach(_zjson_dir IN LISTS _zjson_fallback_include_dirs)
        if(EXISTS "${_zjson_dir}")
            include_directories(SYSTEM "${_zjson_dir}")
        endif()
    endforeach()

    foreach(_zjson_dir IN LISTS _zjson_fallback_lib_dirs)
        if(EXISTS "${_zjson_dir}")
            link_directories("${_zjson_dir}")
        endif()
    endforeach()

    message(STATUS "MSVC implicit include directories missing; using fallback SDK paths")
endif()

if(MSVC AND CMAKE_AR MATCHES [[(^|[\\/])ar(\.exe)?$]])
    get_filename_component(_zjson_msvc_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(CMAKE_AR "${_zjson_msvc_bin_dir}/lib.exe")
    message(STATUS "MSVC archiver mismatch detected; using ${CMAKE_AR}")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

option(CMAKE_ZJSON_ENABLE_ASAN "Enable AddressSanitizer for memory bug detection" OFF)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if(MSVC)
    string(APPEND CMAKE_CXX_FLAGS " /Zc:__cplusplus /utf-8")
    string(APPEND CMAKE_C_FLAGS " /utf-8")
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

##--------------------- Output layout ---------------------------------------##
# Every product of this project (the library demo, the benchmark and the test
# executables) is written to <source>/bin, so a build always leaves its artifacts
# in one predictable place.  Third-party subprojects keep their own default
# locations.  The per-configuration properties are set as well because
# multi-config generators (MSVC) would otherwise append the configuration name.
set(ZJSON_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin" CACHE PATH
    "Directory that receives this project's build products (executables and libraries)")
function(zjson_set_output_dir target)
    set_target_properties(${target} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${ZJSON_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${ZJSON_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${ZJSON_OUTPUT_DIR}"
    )
    foreach(zjson_config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set_target_properties(${target} PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY_${zjson_config} "${ZJSON_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_${zjson_config} "${ZJSON_OUTPUT_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_${zjson_config} "${ZJSON_OUTPUT_DIR}"
        )
    endforeach()
endfunction()

zjson_set_output_dir(${PROJECT_NAME})

##--------------------- Shared dependencies ----------------------------------##
# Both the unit tests and the benchmark use std::thread (cross-thread node pool
# coverage and the concurrency scaling measurement).
find_package(Threads REQUIRED)

##--------------------- Build unit tests -----------------------------------##
option(CMAKE_ZJSON_ENABLE_TEST "Whether to enable unit tests" ON)
if (CMAKE_ZJSON_ENABLE_TEST)
    message(STATUS "Unit tests enabled")
    enable_testing()
    add_subdirectory(thirds/googletest-1.14.0 EXCLUDE_FROM_ALL)
    include_directories(thirds/googletest-1.14.0/googletest/include)

    add_executable(test_test tests/test_test.cpp)
    zjson_set_output_dir(test_test)
    target_link_libraries(test_test gtest gtest_main)
    add_test(NAME test_test COMMAND test_test)

    add_executable(test_object tests/test_object.cpp)
    zjson_set_output_dir(test_object)
    target_link_libraries(test_object gtest gtest_main)
    add_test(NAME test_object COMMAND test_object)

    add_executable(test_array tests/test_array.cpp)
    zjson_set_output_dir(test_array)
    target_link_libraries(test_array gtest gtest_main)
    add_test(NAME test_array COMMAND test_array)

    add_executable(test_parse tests/test_parse.cpp)
    zjson_set_output_dir(test_parse)
    target_link_libraries(test_parse gtest gtest_main)
    add_test(NAME test_parse COMMAND test_parse)

    add_executable(test_more tests/test_more.cpp)
    zjson_set_output_dir(test_more)
    # Lets the test find tests/more.json regardless of the working directory.
    target_compile_definitions(test_more PRIVATE
        ZJSON_TEST_MORE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/more.json")
    target_link_libraries(test_more gtest gtest_main)
    add_test(NAME test_more COMMAND test_more)

    add_executable(test_spec tests/test_spec.cpp)
    zjson_set_output_dir(test_spec)
    target_link_libraries(test_spec gtest gtest_main)
    add_test(NAME test_spec COMMAND test_spec)

    add_executable(test_conformance tests/test_conformance.cpp)
    zjson_set_output_dir(test_conformance)
    target_link_libraries(test_conformance gtest gtest_main)
    add_test(NAME test_conformance COMMAND test_conformance)

    add_executable(test_rfc_cases tests/test_rfc_cases.cpp)
    zjson_set_output_dir(test_rfc_cases)
    target_link_libraries(test_rfc_cases gtest gtest_main)
    add_test(NAME test_rfc_cases COMMAND test_rfc_cases)

    add_executable(test_jsontestsuite tests/test_jsontestsuite.cpp)
    zjson_set_output_dir(test_jsontestsuite)
    target_link_libraries(test_jsontestsuite gtest gtest_main)
    add_test(NAME test_jsontestsuite COMMAND test_jsontestsuite)

    add_executable(test_json_checker tests/test_json_checker.cpp)
    zjson_set_output_dir(test_json_checker)
    target_link_libraries(test_json_checker gtest gtest_main)
    add_test(NAME test_json_checker COMMAND test_json_checker)

    # Full-surface coverage suites: every public API entry point and every
    # documented edge case of the parser / serializer.
    add_executable(test_api_coverage tests/test_api_coverage.cpp)
    zjson_set_output_dir(test_api_coverage)
    target_link_libraries(test_api_coverage gtest gtest_main Threads::Threads)
    add_test(NAME test_api_coverage COMMAND test_api_coverage)

    add_executable(test_parse_coverage tests/test_parse_coverage.cpp)
    zjson_set_output_dir(test_parse_coverage)
    target_link_libraries(test_parse_coverage gtest gtest_main)
    add_test(NAME test_parse_coverage COMMAND test_parse_coverage)

    # Plan-completion suite (R1/R3/R4/R5): reference access, streaming dump,
    # iterative deep-document paths, pointer writes, try_get.
    add_executable(test_advanced tests/test_advanced.cpp)
    zjson_set_output_dir(test_advanced)
    target_link_libraries(test_advanced gtest gtest_main Threads::Threads)
    add_test(NAME test_advanced COMMAND test_advanced)

    # The node pool is intentionally process-lifetime, so LeakSanitizer must be told
    # to ignore it. src/zjson.hpp does that at runtime (__lsan_ignore_object) and
    # tests/lsan.supp is the fallback for toolchains where that path is not compiled.
    if(CMAKE_ZJSON_ENABLE_ASAN AND NOT MSVC AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/lsan.supp")
        get_property(_zjson_registered_tests DIRECTORY PROPERTY TESTS)
        if(_zjson_registered_tests)
            set_tests_properties(${_zjson_registered_tests} PROPERTIES
                ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/tests/lsan.supp")
        endif()
    endif()

endif()

##--------------------- Benchmark -------------------------------------------##
option(CMAKE_ZJSON_ENABLE_BENCH "Whether to build benchmark" ON)
# The comparative benchmark can measure against nlohmann/json, RapidJSON and
# simdjson, but those are only useful when the numbers are being refreshed: the
# library and every unit test build without them, and fetching them costs ~130 MB
# at configure time.  Opt in explicitly with
#   cmake --preset clang-rlz -DCMAKE_ZJSON_BENCH_FETCH_THIRD_PARTY=ON
# when reproducing the comparison tables in docs/性能测试报告.md; otherwise the
# benchmark reports zjson's own numbers only.
option(CMAKE_ZJSON_BENCH_FETCH_THIRD_PARTY "Fetch nlohmann/json, RapidJSON, and simdjson for comparative benchmarks" OFF)
if (CMAKE_ZJSON_ENABLE_BENCH)
    add_executable(bench_zjson bench/bench_main.cpp)
    zjson_set_output_dir(bench_zjson)
    target_link_libraries(bench_zjson PRIVATE Threads::Threads)
    if(MSVC)
        target_compile_options(bench_zjson PRIVATE $<$<CONFIG:Release>:/O2>)
    else()
        target_compile_options(bench_zjson PRIVATE $<$<CONFIG:Release>:-O2>)
    endif()

    if(CMAKE_ZJSON_BENCH_FETCH_THIRD_PARTY)
        include(FetchContent)

        set(JSON_BuildTests OFF CACHE INTERNAL "")
        FetchContent_Declare(
            nlohmann_json
            URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
        FetchContent_MakeAvailable(nlohmann_json)
        target_link_libraries(bench_zjson PRIVATE nlohmann_json::nlohmann_json)
        target_compile_definitions(bench_zjson PRIVATE ZJSON_BENCH_HAS_NLOHMANN=1)

        # RapidJSON has had no release since v1.1.0 (2016), and v1.1.0 does not
        # compile with modern compilers: its GenericStringRef::operator= assigns to
        # const-qualified members, which clang >= 19 and GCC >= 14 reject
        # ("cannot assign to non-static data member 'length' with const-qualified
        # type").  Pin the master commit that removed that ill-formed body, fetched
        # as an archive so the benchmark still uses a reproducible snapshot.
        FetchContent_Declare(
            rapidjson
            URL https://github.com/Tencent/rapidjson/archive/24b5e7a8b27f42fa16b96fc70aade9106cf7102f.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
        FetchContent_GetProperties(rapidjson)
        if(NOT rapidjson_POPULATED)
            FetchContent_Populate(rapidjson)
        endif()
        target_include_directories(bench_zjson PRIVATE "${rapidjson_SOURCE_DIR}/include")
        target_compile_definitions(bench_zjson PRIVATE ZJSON_BENCH_HAS_RAPIDJSON=1)

        set(SIMDJSON_BUILD_STATIC ON CACHE BOOL "" FORCE)
        set(SIMDJSON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(SIMDJSON_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
        set(SIMDJSON_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            simdjson
            GIT_REPOSITORY https://github.com/simdjson/simdjson.git
            GIT_TAG v3.10.1)
        FetchContent_MakeAvailable(simdjson)
        target_link_libraries(bench_zjson PRIVATE simdjson::simdjson)
        target_compile_definitions(bench_zjson PRIVATE ZJSON_BENCH_HAS_SIMDJSON=1)
    endif()
endif()