cmake_minimum_required(VERSION 3.16)
project(RCompiler)
set(CMAKE_CXX_STANDARD 20)

add_compile_options(-O3)

find_package(Boost 1.70.0 REQUIRED COMPONENTS regex)

if(Boost_FOUND)
    message(STATUS "Found Boost: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Libraries: ${Boost_LIBRARIES}")
endif()

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
                         src/semantic.cpp
                         src/irbuilder.cpp
                         src/typemapper.cpp
                         src/builtindeclarator.cpp)

add_executable(code src/semanticmain.cpp)
target_link_libraries(code PRIVATE compiler_lib)

if(Boost_FOUND)
    target_link_libraries(code PRIVATE Boost::regex)
endif()

enable_testing()

add_subdirectory(tests)
add_subdirectory(visualization)
