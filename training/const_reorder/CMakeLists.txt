INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../utils)

set(const_reorder_model_trainer_SRCS constituent_reorder_model.cc trainer.h trainer.cc)
add_executable(const_reorder_model_trainer ${const_reorder_model_trainer_SRCS})
target_link_libraries(const_reorder_model_trainer utils ${Boost_LIBRARIES} z)

set(argument_reorder_model_trainer_SRCS argument_reorder_model.cc trainer.h trainer.cc)
add_executable(argument_reorder_model_trainer ${argument_reorder_model_trainer_SRCS})
target_link_libraries(argument_reorder_model_trainer utils ${Boost_LIBRARIES} z)
