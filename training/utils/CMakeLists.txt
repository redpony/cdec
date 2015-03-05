INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../utils)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../mteval)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../decoder)

find_package(Threads REQUIRED)

set(training_utils_STAT_SRCS
    candidate_set.h
    entropy.h
    lbfgs.h
    online_optimizer.h
    optimize.h
    risk.h
    sentserver.h
    candidate_set.cc
    entropy.cc
    optimize.cc
    online_optimizer.cc
    risk.cc)

add_library(training_utils STATIC ${training_utils_STAT_SRCS})


set(sentserver_SRCS sentserver.cc)
add_executable(sentserver ${sentserver_SRCS})
target_link_libraries(sentserver ${CMAKE_THREAD_LIBS_INIT})

set(sentclient_SRCS sentclient.cc)
add_executable(sentclient ${sentclient_SRCS})
target_link_libraries(sentclient ${CMAKE_THREAD_LIBS_INIT})

set(grammar_convert_SRCS grammar_convert.cc)
add_executable(grammar_convert ${grammar_convert_SRCS})
target_link_libraries(grammar_convert libcdec mteval utils ${Boost_LIBRARIES} z)
