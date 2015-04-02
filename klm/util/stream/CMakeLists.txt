INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../..)

set(klm_util_stream_STAT_SRCS
    block.hh
    chain.cc
    chain.hh
    config.hh
    io.cc
    io.hh
    line_input.cc
    line_input.hh
    multi_progress.cc
    multi_progress.hh
    multi_stream.hh
    sort.hh
    stream.hh
    timer.hh)

add_library(klm_util_stream STATIC ${klm_util_stream_STAT_SRCS})

