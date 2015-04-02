INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/..)

set(ksearch_STAT_SRCS
    applied.hh
    config.hh
    context.hh
    dedupe.hh
    edge.hh
    edge_generator.cc
    edge_generator.hh
    header.hh
    nbest.cc
    nbest.hh
    rule.cc
    rule.hh
    types.hh
    vertex.cc
    vertex.hh
    vertex_generator.hh)

add_library(ksearch STATIC ${ksearch_STAT_SRCS})

