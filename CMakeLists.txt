cmake_minimum_required(VERSION 3.16)
project(RCompiler)
set(CMAKE_CXX_STANDARD 20)

include_directories(include)

add_executable(code src/main.cpp
                    src/lexer.cpp
                    src/parser.cpp
                    src/astnode.cpp)

enable_testing()

add_subdirectory(tests)
