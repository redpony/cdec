INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/..)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../utils)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../utils)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../mteval)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../decoder)

set(minrisk_optimize_SRCS minrisk_optimize.cc)
add_executable(minrisk_optimize ${minrisk_optimize_SRCS})
target_link_libraries(minrisk_optimize training_utils libcdec mteval utils lbfgs ${Boost_LIBRARIES} z)
