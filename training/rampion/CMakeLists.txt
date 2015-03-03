INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../utils)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../utils)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../mteval)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../decoder)

set(rampion_cccp_SRCS rampion_cccp.cc)
add_executable(rampion_cccp ${rampion_cccp_SRCS})
target_link_libraries(rampion_cccp training_utils libcdec mteval utils ${Boost_LIBRARIES} z)
