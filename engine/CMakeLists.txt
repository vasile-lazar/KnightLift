add_executable(KnightLift src/main.cpp)

if(MSVC)
    target_compile_options(KnightLift PRIVATE /W4)
else()
    target_compile_options(KnightLift PRIVATE -Wall -Wextra -Wpedantic)
endif()