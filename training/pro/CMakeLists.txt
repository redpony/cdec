INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../utils)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/..)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../utils)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../mteval)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../decoder)

########### next target ###############

set(mr_pro_map_SRCS mr_pro_map.cc)
add_executable(mr_pro_map ${mr_pro_map_SRCS})
target_link_libraries(mr_pro_map training_utils libcdec ksearch mteval utils klm klm_util klm_util_double ${Boost_LIBRARIES} z)

########### next target ###############

set(mr_pro_reduce_SRCS mr_pro_reduce.cc)
add_executable(mr_pro_reduce ${mr_pro_reduce_SRCS})
target_link_libraries(mr_pro_reduce lbfgs utils ${Boost_LIBRARIES} z)

