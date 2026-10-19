option(KNIGHTLIFT_NATIVE "Compile for the host CPU (-march=native)" ON)

add_executable(KnightLift
        src/main.cpp
        src/bitboard.cpp
        src/position.cpp
        src/attacks.cpp
)

if(MSVC)
    target_compile_options(KnightLift PRIVATE /W4)
else()
    target_compile_options(KnightLift PRIVATE -Wall -Wextra -Wpedantic)
    if(KNIGHTLIFT_NATIVE)
        target_compile_options(KnightLift PRIVATE -march=native)
    endif()
endif()