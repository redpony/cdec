INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../utils)

set(fast_align_SRCS
    fast_align.cc
    ttables.cc
    da.h
    ttables.h)
add_executable(fast_align ${fast_align_SRCS})
target_link_libraries(fast_align utils ${Boost_LIBRARIES} z)

