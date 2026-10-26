add_executable(KnightLift_tests test_main.cpp)
target_compile_definitions(KnightLift_tests PRIVATE
        KNIGHTLIFT_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
target_link_libraries(KnightLift_tests PRIVATE KnightLift_core)
add_test(NAME KnightLift_tests COMMAND KnightLift_tests)