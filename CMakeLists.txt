cmake_minimum_required(VERSION 3.16)
project(PIM_Compiler LANGUAGES CXX)

# The compiler is self-contained C++ and does not use LLVM APIs.  Keeping the
# build independent of a locally installed LLVM makes a fresh checkout
# reproducible (and avoids the old, non-portable ``target_link_libraries(...
# LLVM)`` requirement).
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(PIM_Compiler
    src/main.cpp
    src/Lexer.cpp
    src/Parser.cpp
    src/CodeGen.cpp
    src/TargetBackend.cpp
)

enable_testing()
foreach(test_case IN ITEMS test1 test2 test3 test4 test5)
    add_test(
        NAME compile_${test_case}
        COMMAND PIM_Compiler
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/${test_case}.cpp
                -o ${CMAKE_CURRENT_BINARY_DIR}/${test_case}.isa
    )
endforeach()

target_include_directories(PIM_Compiler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_options(PIM_Compiler PRIVATE /W4)
else()
    target_compile_options(PIM_Compiler PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS PIM_Compiler RUNTIME DESTINATION bin)
