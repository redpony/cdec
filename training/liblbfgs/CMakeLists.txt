INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/..)


set(lbfgs_STAT_SRCS
    lbfgs.c
    arithmetic_ansi.h
    arithmetic_sse_double.h
    arithmetic_sse_float.h
    lbfgs++.h
    lbfgs.h)

add_library(lbfgs STATIC ${lbfgs_STAT_SRCS})

set(TEST_SRCS ll_test.cc)

foreach(testSrc ${TEST_SRCS})
  #Extract the filename without an extension (NAME_WE)
  get_filename_component(testName ${testSrc} NAME_WE)

  #Add compile target
  set_source_files_properties(${testSrc} PROPERTIES COMPILE_FLAGS "-DBOOST_TEST_DYN_LINK -DTEST_DATA=\\\"test_data/\\\"")
  add_executable(${testName} ${testSrc})

  #link to Boost libraries AND your targets and dependencies
  target_link_libraries(${testName} lbfgs ${Boost_LIBRARIES} ${ZLIB_LIBRARIES})

  #I like to move testing binaries into a testBin directory
  set_target_properties(${testName} PROPERTIES 
      RUNTIME_OUTPUT_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR})

  #Finally add it to test execution - 
  #Notice the WORKING_DIRECTORY and COMMAND
  add_test(NAME ${testName} COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/${testName} 
     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach(testSrc)


