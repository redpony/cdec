cmake_minimum_required(VERSION 2.8)
project(cdec)

add_definitions(-DKENLM_MAX_ORDER=6 -DHAVE_CONFIG_H)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
set(CMAKE_CXX_FLAGS "-Wall -std=c++11 -O3 -g")
set(METEOR_JAR "" CACHE FILEPATH "Path to meteor.jar")

enable_testing()
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# core packages
find_package(LibDL REQUIRED)
if(DEFINED ENV{BOOST_ROOT})
  set(Boost_NO_SYSTEM_PATHS ON)
endif()
set(Boost_REALPATH ON)
find_package(Boost COMPONENTS regex filesystem serialization program_options unit_test_framework system thread REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

# eigen, used in some modeling extensions
find_package(Eigen3)
if(EIGEN3_FOUND)
  include_directories(${EIGEN3_INCLUDE_DIR})
  set(HAVE_EIGEN 1)
endif(EIGEN3_FOUND)

# compression packages (primarily used by KenLM)
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

# for pycdec
find_package(PythonInterp 2.7 REQUIRED)

# generate config.h
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

set(CPACK_PACKAGE_VERSION_MAJOR "2015")
set(CPACK_PACKAGE_VERSION_MINOR "04")
set(CPACK_PACKAGE_VERSION_PATCH "26")
set(CPACK_SOURCE_GENERATOR "TBZ2")
set(CPACK_SOURCE_PACKAGE_FILE_NAME
  "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_SOURCE_IGNORE_FILES
  "/.git/;/.gitignore;/Testing/;/build/;/.bzr/;~$;/CMakeCache.txt;/CMakeFiles/;${CPACK_SOURCE_IGNORE_FILES}")
include(CPack)

