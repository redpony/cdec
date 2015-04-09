cmake_minimum_required(VERSION 2.8)

add_definitions(-DKENLM_MAX_ORDER=6 -DHAVE_CONFIG_H)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
set(CMAKE_CXX_FLAGS "-Wall -std=c++11 -O3")
set(METEOR_JAR "" CACHE FILEPATH "Path to meteor.jar")

enable_testing()
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

#### packages
find_package(ZLIB REQUIRED)
if(ZLIB_FOUND)
  set(HAVE_ZLIB 1)
endif(ZLIB_FOUND)
find_package(BZip2)
if(BZIP2_FOUND)
  set(HAVE_BZLIB 1)
endif(BZIP2_FOUND)
find_package(LibLZMA)
if(LIBLZMA_FOUND)
  set(HAVE_XZLIB 1)
endif(LIBLZMA_FOUND)

find_package(LibDL REQUIRED)

# for pycdec
find_package(PythonInterp 2.7 REQUIRED)

find_package(Boost COMPONENTS regex filesystem serialization program_options unit_test_framework system thread REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)

add_subdirectory(utils)
add_subdirectory(klm/util/double-conversion)
add_subdirectory(klm/util)
add_subdirectory(klm/util/stream)
add_subdirectory(klm/lm)
add_subdirectory(klm/lm/builder)
add_subdirectory(klm/search)
add_subdirectory(mteval)
add_subdirectory(decoder)
add_subdirectory(training)
add_subdirectory(word-aligner)
add_subdirectory(extractor)
add_subdirectory(example_extff)

