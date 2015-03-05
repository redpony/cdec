INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../utils)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../utils)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../mteval)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../decoder)

set(latent_svm_SRCS latent_svm.cc)
add_executable(latent_svm ${latent_svm_SRCS})
target_link_libraries(latent_svm libcdec ksearch mteval utils klm klm_util klm_util_double ${Boost_LIBRARIES} z ${LIBDL_LIBRARIES})
