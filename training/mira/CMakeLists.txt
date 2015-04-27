INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../utils)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../utils)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../mteval)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../decoder)

set(kbest_mira_SRCS kbest_mira.cc)
add_executable(kbest_mira ${kbest_mira_SRCS})
target_link_libraries(kbest_mira libcdec ksearch mteval utils klm klm_util klm_util_double ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} ${LIBLZMA_LIBRARIES} ${LIBDL_LIBRARIES})

set(kbest_cut_mira_SRCS kbest_cut_mira.cc)
add_executable(kbest_cut_mira ${kbest_cut_mira_SRCS})
target_link_libraries(kbest_cut_mira libcdec ksearch mteval utils klm klm_util klm_util_double ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} ${LIBLZMA_LIBRARIES} ${LIBDL_LIBRARIES})

add_custom_command(TARGET kbest_cut_mira
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/mira.py ${CMAKE_CURRENT_BINARY_DIR}/mira.py)

set(ada_opt_sm_SRCS ada_opt_sm.cc)
add_executable(ada_opt_sm ${ada_opt_sm_SRCS})
target_link_libraries(ada_opt_sm training_utils libcdec ksearch mteval utils klm klm_util klm_util_double ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} ${LIBLZMA_LIBRARIES} ${LIBDL_LIBRARIES})
