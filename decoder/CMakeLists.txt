INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../utils)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../mteval)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../klm)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/..)

PROJECT(decoder CXX)

if (CMAKE_VERSION VERSION_LESS 2.8.9) # TODO remove once we increase the cmake requirement
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -DPIC")
else()
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

find_package(FLEX REQUIRED)

FLEX_TARGET(RuleLexer rule_lexer.ll ${CMAKE_CURRENT_BINARY_DIR}/rule_lexer.cc)

set(libcdec_SRCS
    aligner.h
    apply_models.h
    bottom_up_parser.h
    bottom_up_parser-rs.h
    csplit.h
    decoder.h
    earley_composer.h
    factored_lexicon_helper.h
    ff.h
    ff_basic.h
    ff_bleu.h
    ff_charset.h
    ff_conll.h
    ff_const_reorder_common.h
    ff_const_reorder.h
    ff_context.h
    ff_csplit.h
    ff_external.h
    ff_factory.h
    ff_klm.h
    ff_lexical.h
    ff_lm.h
    ff_ngrams.h
    ff_parse_match.h
    ff_register.h
    ff_rules.h
    ff_ruleshape.h
    ff_sample_fsa.h
    ff_soft_syn.h
    ff_soft_syntax.h
    ff_soft_syntax_mindist.h
    ff_source_path.h
    ff_source_syntax.h
    ff_source_syntax2.h
    ff_spans.h
    ff_tagger.h
    ff_wordalign.h
    ff_wordset.h
    ffset.h
    forest_writer.h
    freqdict.h
    grammar.h
    hg.h
    hg_intersect.h
    hg_io.h
    hg_remove_eps.h
    hg_sampler.h
    hg_test.h
    hg_union.h
    incremental.h
    inside_outside.h
    kbest.h
    lattice.h
    lexalign.h
    lextrans.h
    nt_span.h
    oracle_bleu.h
    phrasebased_translator.h
    phrasetable_fst.h
    program_options.h
    rule_lexer.h
    sentence_metadata.h
    sentences.h
    tagger.h
    translator.h
    trule.h
    viterbi.h
    aligner.cc
    apply_models.cc
    bottom_up_parser.cc
    bottom_up_parser-rs.cc
    cdec_ff.cc
    csplit.cc
    decoder.cc
    earley_composer.cc
    factored_lexicon_helper.cc
    ff.cc
    ff_basic.cc
    ff_bleu.cc
    ff_charset.cc
    ff_conll.cc
    ff_context.cc
    ff_const_reorder.cc
    ff_csplit.cc
    ff_external.cc
    ff_factory.cc
    ff_klm.cc
    ff_lm.cc
    ff_ngrams.cc
    ff_parse_match.cc
    ff_rules.cc
    ff_ruleshape.cc
    ff_soft_syn.cc
    ff_soft_syntax.cc
    ff_soft_syntax_mindist.cc
    ff_source_path.cc
    ff_source_syntax.cc
    ff_source_syntax2.cc
    ff_spans.cc
    ff_tagger.cc
    ff_wordalign.cc
    ff_wordset.cc
    ffset.cc
    forest_writer.cc
    fst_translator.cc
    tree2string_translator.cc
    grammar.cc
    hg.cc
    hg_intersect.cc
    hg_io.cc
    hg_remove_eps.cc
    hg_sampler.cc
    hg_union.cc
    incremental.cc
    lattice.cc
    lexalign.cc
    lextrans.cc
    node_state_hash.h
    tree_fragment.cc
    tree_fragment.h
    maxtrans_blunsom.cc
    phrasebased_translator.cc
    phrasetable_fst.cc
    rescore_translator.cc
    ${FLEX_RuleLexer_OUTPUTS}
    scfg_translator.cc
    tagger.cc
    translator.cc
    trule.cc
    viterbi.cc)

add_library(libcdec STATIC ${libcdec_SRCS})

set(cdec_SRCS cdec.cc)
add_executable(cdec ${cdec_SRCS})
target_link_libraries(cdec libcdec mteval utils ksearch klm klm_util klm_util_double ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} ${LIBLZMA_LIBRARIES} ${LIBDL_LIBRARIES})

set(TEST_SRCS
  grammar_test.cc
  hg_test.cc
  parser_test.cc
  t2s_test.cc
  trule_test.cc)

foreach(testSrc ${TEST_SRCS})
  #Extract the filename without an extension (NAME_WE)
  get_filename_component(testName ${testSrc} NAME_WE)

  #Add compile target
  set_source_files_properties(${testSrc} PROPERTIES COMPILE_FLAGS "-DBOOST_TEST_DYN_LINK -DTEST_DATA=\\\"test_data/\\\"")
  add_executable(${testName} ${testSrc})

  #link to Boost libraries AND your targets and dependencies
  target_link_libraries(${testName} libcdec mteval utils ksearch klm klm_util klm_util_double ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} ${LIBLZMA_LIBRARIES})

  #I like to move testing binaries into a testBin directory
  set_target_properties(${testName} PROPERTIES 
      RUNTIME_OUTPUT_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR})

  #Finally add it to test execution - 
  #Notice the WORKING_DIRECTORY and COMMAND
  add_test(NAME ${testName} COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/${testName} 
     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach(testSrc)

