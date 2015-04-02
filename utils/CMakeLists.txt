if (CMAKE_VERSION VERSION_LESS 2.8.9) # TODO remove once we increase the cmake requirement
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -DPIC")
else()
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

set(TEST_SRCS dict_test.cc
  logval_test.cc
  m_test.cc
  small_vector_test.cc
  stringlib_test.cc
  sv_test.cc
  ts.cc
  weights_test.cc)

foreach(testSrc ${TEST_SRCS})
  #Extract the filename without an extension (NAME_WE)
  get_filename_component(testName ${testSrc} NAME_WE)

  #Add compile target
  set_source_files_properties(${testSrc} PROPERTIES COMPILE_FLAGS "-DBOOST_TEST_DYN_LINK -DTEST_DATA=\\\"test_data/\\\"")
  add_executable(${testName} ${testSrc})

  #link to Boost libraries AND your targets and dependencies
  target_link_libraries(${testName} utils ${Boost_LIBRARIES} ${ZLIB_LIBRARIES})

  #I like to move testing binaries into a testBin directory
  set_target_properties(${testName} PROPERTIES 
      RUNTIME_OUTPUT_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR})

  #Finally add it to test execution - 
  #Notice the WORKING_DIRECTORY and COMMAND
  add_test(NAME ${testName} COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/${testName} 
     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach(testSrc)


set(atools_SRCS atools.cc)
add_executable(atools ${atools_SRCS})
target_link_libraries(atools utils ${Boost_LIBRARIES} ${ZLIB_LIBRARIES})

set(dedup_corpus_SRCS dedup_corpus.cc)
add_executable(dedup_corpus ${dedup_corpus_SRCS})
target_link_libraries(dedup_corpus utils ${Boost_LIBRARIES} ${ZLIB_LIBRARIES})

set(utils_STAT_SRCS
    test_data
    alias_sampler.h
    alignment_io.h
    array2d.h
    b64featvector.h
    b64tools.h
    batched_append.h
    city.h
    citycrc.h
    corpus_tools.h
    dict.h
    exp_semiring.h
    fast_sparse_vector.h
    fdict.h
    feature_vector.h
    filelib.h
    gzstream.h
    hash.h
    have_64_bits.h
    indices_after.h
    kernel_string_subseq.h
    logval.h
    m.h
    maxent.h
    maxent.cpp
    murmur_hash3.h
    murmur_hash3.cc
    named_enum.h
    null_deleter.h
    null_traits.h
    prob.h
    sampler.h
    semiring.h
    show.h
    small_vector.h
    sparse_vector.h
    star.h
    static_utoa.h
    stringlib.h
    string_piece.hh
    tdict.h
    timing_stats.h
    utoa.h
    value_array.h
    verbose.h
    warning_compiler.h
    warning_pop.h
    warning_push.h
    weights.h
    wordid.h
    writer.h
    fast_lexical_cast.hpp
    intrusive_refcount.hpp
    alignment_io.cc
    b64featvector.cc
    b64tools.cc
    corpus_tools.cc
    dict.cc
    tdict.cc
    fdict.cc
    gzstream.cc
    filelib.cc
    stringlib.cc
    string_piece.cc
    sparse_vector.cc
    timing_stats.cc
    verbose.cc
    weights.cc)

add_library(utils STATIC ${utils_STAT_SRCS})


