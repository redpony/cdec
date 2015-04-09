INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/..)

set(cat_compressed_SRCS cat_compressed_main.cc)

add_executable(cat_compressed ${cat_compressed_SRCS})
target_link_libraries(cat_compressed klm_util ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} ${LIBLZMA_LIBRARIES})

set(klm_util_STAT_SRCS
    bit_packing.cc
    bit_packing.hh
    ersatz_progress.cc
    ersatz_progress.hh
    exception.cc
    exception.hh
    fake_ofstream.hh
    file.cc
    file.hh
    file_piece.cc
    file_piece.hh
    fixed_array.hh
    getopt.c
    getopt.hh
    have.hh
    joint_sort.hh
    mmap.cc
    mmap.hh
    multi_intersection.hh
    murmur_hash.cc
    murmur_hash.hh
    parallel_read.cc
    parallel_read.hh
    pcqueue.hh
    pool.cc
    pool.hh
    probing_hash_table.hh
    proxy_iterator.hh
    read_compressed.cc
    read_compressed.hh
    scoped.cc
    scoped.hh
    sized_iterator.hh
    sorted_uniform.hh
    string_piece.cc
    string_piece.hh
    string_piece_hash.hh
    thread_pool.hh
    tokenize_piece.hh
    unistd.hh
    usage.cc
    usage.hh)

add_library(klm_util STATIC ${klm_util_STAT_SRCS})

