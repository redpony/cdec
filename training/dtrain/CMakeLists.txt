INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../utils)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../utils)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../mteval)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../decoder)

set(dtrain_SRCS
    dtrain.cc
    score.cc
    dtrain.h
    kbestget.h
    ksampler.h
    pairsampling.h
    score.h)
add_executable(dtrain ${dtrain_SRCS})
target_link_libraries(dtrain libcdec ksearch mteval utils klm klm_util klm_util_double ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} ${LIBLZMA_LIBRARIES} ${LIBDL_LIBRARIES})


add_custom_command(TARGET dtrain
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/lplp.rb ${CMAKE_CURRENT_BINARY_DIR})

add_custom_command(TARGET dtrain
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/parallelize.rb ${CMAKE_CURRENT_BINARY_DIR})


