INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../utils)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../utils)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../mteval)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../decoder)

set(dpmert_SRCS
    mert_geometry.cc
    ces.cc
    error_surface.cc
    mr_dpmert_map.cc
    line_optimizer.cc
    ces.h
    error_surface.h
    line_optimizer.h
    mert_geometry.h)
add_library(dpmert ${dpmert_SRCS})

set(mr_dpmert_map_SRCS mr_dpmert_map.cc)

add_executable(mr_dpmert_map ${mr_dpmert_map_SRCS})
target_link_libraries(mr_dpmert_map dpmert training_utils libcdec ksearch mteval utils klm klm_util klm_util_double ${Boost_LIBRARIES} z)

########### next target ###############

set(mr_dpmert_reduce_SRCS
    mr_dpmert_reduce.cc)

add_executable(mr_dpmert_reduce ${mr_dpmert_reduce_SRCS})
target_link_libraries(mr_dpmert_reduce dpmert training_utils libcdec ksearch mteval utils klm klm_util klm_util_double ${Boost_LIBRARIES} z)

add_custom_command(TARGET mr_dpmert_reduce
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/dpmert.pl ${CMAKE_CURRENT_BINARY_DIR})

add_custom_command(TARGET mr_dpmert_reduce
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/divide_refs.py ${CMAKE_CURRENT_BINARY_DIR})


set(mr_dpmert_generate_mapper_input_SRCS mr_dpmert_generate_mapper_input.cc)
add_executable(mr_dpmert_generate_mapper_input ${mr_dpmert_generate_mapper_input_SRCS})
target_link_libraries(mr_dpmert_generate_mapper_input dpmert training_utils libcdec ksearch mteval utils klm klm_util klm_util_double ${Boost_LIBRARIES} z)

set(TEST_SRCS lo_test.cc)
foreach(testSrc ${TEST_SRCS})
  #Extract the filename without an extension (NAME_WE)
  get_filename_component(testName ${testSrc} NAME_WE)

  #Add compile target
  set_source_files_properties(${testSrc} PROPERTIES COMPILE_FLAGS "-DBOOST_TEST_DYN_LINK -DTEST_DATA=\\\"test_data/\\\"")
  add_executable(${testName} ${testSrc})

  #link to Boost libraries AND your targets and dependencies
  target_link_libraries(${testName} dpmert training_utils libcdec ksearch mteval utils klm klm_util klm_util_double ${Boost_LIBRARIES} ${ZLIB_LIBRARIES})

  #I like to move testing binaries into a testBin directory
  set_target_properties(${testName} PROPERTIES 
      RUNTIME_OUTPUT_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR})

  #Finally add it to test execution - 
  #Notice the WORKING_DIRECTORY and COMMAND
  add_test(NAME ${testName} COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/${testName} 
     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach(testSrc)



