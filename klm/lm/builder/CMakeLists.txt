INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../..)

set(lmplz_SRCS
    lmplz_main.cc
    adjust_counts.cc
    adjust_counts.hh
    corpus_count.cc
    corpus_count.hh
    discount.hh
    hash_gamma.hh
    header_info.hh
    initial_probabilities.cc
    initial_probabilities.hh
    interpolate.cc
    interpolate.hh
    joint_order.hh
    multi_stream.hh
    ngram.hh
    ngram_stream.hh
    pipeline.cc
    pipeline.hh
    print.cc
    print.hh
    sort.hh)

add_executable(lmplz ${lmplz_SRCS})
target_link_libraries(lmplz klm klm_util_double klm_util_stream klm_util ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} ${LIBLZMA_LIBRARIES})

set(dump_counts_SRCS print.cc dump_counts_main.cc)
add_executable(dump_counts ${dump_counts_SRCS})
target_link_libraries(dump_counts klm klm_util_double klm_util_stream klm_util ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} ${LIBLZMA_LIBRARIES})
