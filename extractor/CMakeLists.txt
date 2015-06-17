INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../utils)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/features)

find_package(OpenMP)
if (OPENMP_FOUND)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

find_package(GTest)
find_package(GMock)
if(GTEST_FOUND)
 if(GMOCK_FOUND)
  #rule_factory_test.cc
  set(TEST_SRCS alignment_test.cc
    data_array_test.cc
    fast_intersector_test.cc
    grammar_extractor_test.cc
    matchings_finder_test.cc
    matchings_sampler_test.cc
    phrase_location_sampler_test.cc
    phrase_test.cc
    precomputation_test.cc
    rule_extractor_helper_test.cc
    rule_extractor_test.cc
    scorer2_test.cc
    suffix_array_sampler_test.cc
    suffix_array_test.cc
    target_phrase_extractor_test.cc
    translation_table_test.cc
    vocabulary_test.cc)
  INCLUDE_DIRECTORIES(${GTEST_INCLUDE_DIRS})
  INCLUDE_DIRECTORIES(${GMOCK_INCLUDE_DIRS})
  foreach(testSrc ${TEST_SRCS})
    #Extract the filename without an extension (NAME_WE)
    get_filename_component(testName ${testSrc} NAME_WE)

    #Add compile target
    add_executable(${testName} ${testSrc})

    #link to Boost libraries AND your targets and dependencies
    target_link_libraries(${testName} extractor ${GMOCK_BOTH_LIBRARIES} ${GTEST_BOTH_LIBRARIES} ${Boost_LIBRARIES} ${ZLIB_LIBRARIES})

    #I like to move testing binaries into a testBin directory
    set_target_properties(${testName} PROPERTIES 
      RUNTIME_OUTPUT_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR})

    #Finally add it to test execution - 
    #Notice the WORKING_DIRECTORY and COMMAND
    add_test(NAME ${testName} COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/${testName} 
       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  endforeach(testSrc)
 endif(GMOCK_FOUND)
endif(GTEST_FOUND)

set(sacompile_SRCS sacompile.cc)
add_executable(sacompile ${sacompile_SRCS})
target_link_libraries(sacompile extractor utils ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} ${LIBLZMA_LIBRARIES})

set(run_extractor_SRCS run_extractor.cc)
add_executable(run_extractor ${run_extractor_SRCS})
target_link_libraries(run_extractor extractor utils ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} ${LIBLZMA_LIBRARIES})

set(extract_SRCS extract.cc)
add_executable(extract ${extract_SRCS})
target_link_libraries(extract extractor utils ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} ${LIBLZMA_LIBRARIES})

set(extractor_STAT_SRCS
    alignment.cc
    backoff_sampler.cc
    data_array.cc
    fast_intersector.cc
    features/count_source_target.cc
    features/feature.cc
    features/is_source_singleton.cc
    features/is_source_target_singleton.cc
    features/max_lex_source_given_target.cc
    features/max_lex_target_given_source.cc
    features/sample_source_count.cc
    features/target_given_source_coherent.cc
    features/count_source_target.h
    features/feature.h
    features/is_source_singleton.h
    features/is_source_target_singleton.h
    features/max_lex_source_given_target.h
    features/max_lex_target_given_source.h
    features/sample_source_count.h
    features/target_given_source_coherent.h
    grammar.cc
    grammar_extractor.cc
    matchings_finder.cc
    matchings_sampler.cc
    matchings_trie.cc
    phrase.cc
    phrase_builder.cc
    phrase_location.cc
    phrase_location_sampler.cc
    precomputation.cc
    rule.cc
    rule_extractor.cc
    rule_extractor_helper.cc
    rule_factory.cc
    scorer.cc
    suffix_array.cc
    suffix_array_sampler.cc
    target_phrase_extractor.cc
    time_util.cc
    translation_table.cc
    vocabulary.cc
    alignment.h
    backoff_sampler.h
    data_array.h
    fast_intersector.h
    grammar.h
    grammar_extractor.h
    matchings_finder.h
    matchings_sampler.h
    matchings_trie.h
    phrase.h
    phrase_builder.h
    phrase_location.h
    phrase_location_sampler.h
    precomputation.h
    rule.h
    rule_extractor.h
    rule_extractor_helper.h
    rule_factory.h
    sampler.h
    scorer.h
    suffix_array.h
    suffix_array_sampler.h
    target_phrase_extractor.h
    time_util.h
    translation_table.h
    vocabulary.h)

add_library(extractor STATIC ${extractor_STAT_SRCS})

