cmake_minimum_required(VERSION 3.16)
project(RCompiler)
set(CMAKE_CXX_STANDARD 20)

include_directories(include)

add_library(compiler_lib src/lexer.cpp
                         src/parser.cpp
                         src/astnode.cpp
                         src/symboltable.cpp
                         src/symbolcollection.cpp
                         src/typecheck.cpp
                         src/constevaluator.cpp
                         src/controlflow.cpp
                         src/typeinference.cpp
                         src/semantic.cpp)

add_executable(code src/semanticmain.cpp)
target_link_libraries(code compiler_lib)

enable_testing()

add_subdirectory(tests)
