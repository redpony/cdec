INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../utils)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../utils)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../mteval)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../decoder)



########### next target ###############

set(mpi_batch_optimize_SRCS mpi_batch_optimize.cc cllh_observer.cc cllh_observer.h)
add_executable(mpi_batch_optimize ${mpi_batch_optimize_SRCS})
target_link_libraries(mpi_batch_optimize training_utils libcdec ksearch mteval utils klm klm_util klm_util_double ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} ${LIBLZMA_LIBRARIES} ${LIBDL_LIBRARIES})

########### next target ###############

set(mpi_adagrad_optimize_SRCS mpi_adagrad_optimize.cc cllh_observer.cc cllh_observer.h)

add_executable(mpi_adagrad_optimize ${mpi_adagrad_optimize_SRCS})

target_link_libraries(mpi_adagrad_optimize ${KDE4_KDECORE_LIBS} training_utils libcdec ksearch mteval utils klm klm_util klm_util_double ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} ${LIBLZMA_LIBRARIES} ${LIBDL_LIBRARIES})


########### next target ###############

set(mpi_compute_cllh_SRCS mpi_compute_cllh.cc cllh_observer.cc cllh_observer.h)

add_executable(mpi_compute_cllh ${mpi_compute_cllh_SRCS})

target_link_libraries(mpi_compute_cllh ${KDE4_KDECORE_LIBS} libcdec ksearch mteval utils klm klm_util klm_util_double ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} ${LIBLZMA_LIBRARIES} ${LIBDL_LIBRARIES})


########### next target ###############

set(mpi_extract_features_SRCS mpi_extract_features.cc)

add_executable(mpi_extract_features ${mpi_extract_features_SRCS})

target_link_libraries(mpi_extract_features ${KDE4_KDECORE_LIBS} libcdec ksearch mteval utils klm klm_util klm_util_double ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} ${LIBLZMA_LIBRARIES} ${LIBDL_LIBRARIES})


########### next target ###############

set(mpi_extract_reachable_SRCS mpi_extract_reachable.cc)

add_executable(mpi_extract_reachable ${mpi_extract_reachable_SRCS})

target_link_libraries(mpi_extract_reachable ${KDE4_KDECORE_LIBS} libcdec ksearch mteval utils klm klm_util klm_util_double ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} ${LIBLZMA_LIBRARIES} ${LIBDL_LIBRARIES})

########### next target ###############

set(mpi_flex_optimize_SRCS mpi_flex_optimize.cc)

add_executable(mpi_flex_optimize ${mpi_flex_optimize_SRCS})

target_link_libraries(mpi_flex_optimize ${KDE4_KDECORE_LIBS} training_utils libcdec ksearch mteval utils klm klm_util klm_util_double ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} ${LIBLZMA_LIBRARIES} ${LIBDL_LIBRARIES})

########### next target ###############

set(mpi_online_optimize_SRCS mpi_online_optimize.cc)

add_executable(mpi_online_optimize ${mpi_online_optimize_SRCS})

target_link_libraries(mpi_online_optimize ${KDE4_KDECORE_LIBS} training_utils libcdec ksearch mteval utils klm klm_util klm_util_double ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} ${LIBLZMA_LIBRARIES} ${LIBDL_LIBRARIES})

########### next target ###############

set(mpi_baum_welch_SRCS mpi_baum_welch.cc)

add_executable(mpi_baum_welch ${mpi_baum_welch_SRCS})

target_link_libraries(mpi_baum_welch ${KDE4_KDECORE_LIBS} libcdec ksearch mteval utils klm klm_util klm_util_double ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} ${LIBLZMA_LIBRARIES} ${LIBDL_LIBRARIES})

