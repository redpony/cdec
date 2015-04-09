INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../utils)

set(fast_score_SRCS fast_score.cc)
add_executable(fast_score ${fast_score_SRCS})
target_link_libraries(fast_score mteval utils ${Boost_LIBRARIES} ${ZLIB_LIBRARIES})

set(mbr_kbest_SRCS mbr_kbest.cc)
add_executable(mbr_kbest ${mbr_kbest_SRCS})
target_link_libraries(mbr_kbest mteval utils ${Boost_LIBRARIES} ${ZLIB_LIBRARIES})

set(TEST_SRCS scorer_test.cc)

foreach(testSrc ${TEST_SRCS})
  #Extract the filename without an extension (NAME_WE)
  get_filename_component(testName ${testSrc} NAME_WE)

  #Add compile target
  set_source_files_properties(${testSrc} PROPERTIES COMPILE_FLAGS "-DBOOST_TEST_DYN_LINK -DTEST_DATA=\\\"test_data/\\\"")
  add_executable(${testName} ${testSrc})

  #link to Boost libraries AND your targets and dependencies
  target_link_libraries(${testName} mteval utils ${Boost_LIBRARIES} ${ZLIB_LIBRARIES})

  #I like to move testing binaries into a testBin directory
  set_target_properties(${testName} PROPERTIES 
      RUNTIME_OUTPUT_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR})

  #Finally add it to test execution - 
  #Notice the WORKING_DIRECTORY and COMMAND
  add_test(NAME ${testName} COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/${testName} 
     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach(testSrc)



set(mteval_STAT_SRCS
    aer_scorer.h
    comb_scorer.h
    external_scorer.h
    levenshtein.h
    ns.h
    ns_cer.h
    ns_comb.h
    ns_docscorer.h
    ns_ext.h
    ns_ssk.h
    ns_ter.h
    ns_wer.h
    scorer.h
    ter.h
    aer_scorer.cc
    comb_scorer.cc
    external_scorer.cc
    meteor_jar.cc
    wer.cc
    ns.cc
    ns_cer.cc
    ns_comb.cc
    ns_docscorer.cc
    ns_ext.cc
    ns_ssk.cc
    ns_ter.cc
    ns_wer.cc
    scorer.cc
    ter.cc)

add_library(mteval STATIC ${mteval_STAT_SRCS})


