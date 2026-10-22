find_package(Threads REQUIRED)

add_executable(KnightLift_tune tune.cpp)
target_link_libraries(KnightLift_tune PRIVATE KnightLift_core Threads::Threads)