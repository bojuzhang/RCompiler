# AST图形化可视化工具的CMake配置
cmake_minimum_required(VERSION 3.16)
project(ASTGraphVisualizer)

# 添加可执行文件
add_executable(ast_graph_visualizer
    ast_graph_visualizer_main.cpp
    ast_graph_visualizer.cpp
)

# 设置包含目录
target_include_directories(ast_graph_visualizer PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# 链接必要的库
target_link_libraries(ast_graph_visualizer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../build/libcompiler_lib.a
)

# 设置C++标准
set_property(TARGET ast_graph_visualizer PROPERTY CXX_STANDARD 20)
set_property(TARGET ast_graph_visualizer PROPERTY CXX_STANDARD_REQUIRED ON)

# 编译选项
if(MSVC)
    target_compile_options(ast_graph_visualizer PRIVATE /W4)
else()
    target_compile_options(ast_graph_visualizer PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 安装规则
install(TARGETS ast_graph_visualizer
    RUNTIME DESTINATION bin
)

# 如果需要，可以添加自定义的Graphviz路径检查
find_program(GRAPHVIZ_DOT_EXECUTABLE dot
    DOC "Graphviz dot executable"
)

if(GRAPHVIZ_DOT_EXECUTABLE)
    message(STATUS "Found Graphviz dot: ${GRAPHVIZ_DOT_EXECUTABLE}")
    target_compile_definitions(ast_graph_visualizer PRIVATE HAVE_GRAPHVIZ)
else()
    message(WARNING "Graphviz dot not found. PNG/SVG rendering will not work.")
    message(WARNING "Please install Graphviz: https://graphviz.org/download/")
endif()