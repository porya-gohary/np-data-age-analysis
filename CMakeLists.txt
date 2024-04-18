cmake_minimum_required(VERSION 3.8.0)

project(NP_data_age_analysis VERSION 1.2.0 LANGUAGES CXX)

include_directories(include)
include_directories(lib/include)

option(USE_TBB_MALLOC "Use the Intel TBB scalable memory allocator" OFF)
option(USE_JE_MALLOC "Use the Facebook jemalloc scalable memory allocator" OFF)
option(COLLECT_SCHEDULE_GRAPHS "Enable the collection of schedule graphs (disables parallel)" OFF)
option(DEBUG "Enable debugging" OFF)

if (DEBUG)
    set(CMAKE_BUILD_TYPE Debug)
    message(NOTICE "Debug build")
else ()
    set(CMAKE_BUILD_TYPE Release)
endif ()

if (USE_TBB_MALLOC)
    find_package(TBB REQUIRED)
endif()

if (COLLECT_SCHEDULE_GRAPHS)
    add_compile_definitions(CONFIG_COLLECT_SCHEDULE_GRAPH)
endif ()

if (USE_JE_MALLOC)
    find_library(ALLOC_LIB NAMES jemalloc)
    message(NOTICE "Using Facebook jemalloc scalable memory allocator")
elseif (USE_TBB_MALLOC)
    set(ALLOC_LIB TBB::tbbmalloc_proxy)
    message(NOTICE "Using Intel TBB scalable memory allocator")
endif ()

# Check if yaml-cpp is installed as a system library
find_package(yaml-cpp)
if (NOT yaml-cpp_FOUND)
    # Include yaml-cpp source directory and add it to the build
    message(NOTICE "yaml-cpp not found: adding it to the build as a subdirectory")
    add_subdirectory(lib/yaml-cpp)
else ()
    message(NOTICE "yaml-cpp found")
endif ()

set(YAML_LIB yaml-cpp)
set(CORE_LIBS ${TBB_LIB} ${ALLOC_LIB} ${YAML_LIB})


set(ANALYSIS_SOURCES src/run_analysis.cpp lib/src/OptionParser.cpp)
add_executable(run_analysis ${ANALYSIS_SOURCES})

target_link_libraries(run_analysis ${CORE_LIBS})

target_compile_features(run_analysis PUBLIC cxx_std_14)

if (MSVC)
    target_compile_options(run_analysis PUBLIC "/Zc:__cplusplus")
endif()