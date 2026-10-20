option(KNIGHTLIFT_NATIVE "Compile for the host CPU (-march=native)" ON)

add_library(KnightLift_core STATIC
        src/bitboard.cpp
        src/position.cpp
        src/attacks.cpp
        src/movegen.cpp
        src/perft.cpp
        src/uci.cpp
)
target_include_directories(KnightLift_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(MSVC)
    target_compile_options(KnightLift_core PUBLIC /W4)
else()
    target_compile_options(KnightLift_core PUBLIC -Wall -Wextra -Wpedantic)
    if(KNIGHTLIFT_NATIVE)
        target_compile_options(KnightLift_core PUBLIC -march=native)
    endif()
endif()

add_executable(KnightLift src/main.cpp)
target_link_libraries(KnightLift PRIVATE KnightLift_core)
if(MINGW)
    target_link_options(KnightLift PRIVATE -static)
endif()