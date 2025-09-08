# 查找 GoogleTest
find_package(GTest REQUIRED)

# 添加测试可执行文件
add_executable(lexer_test lexer_test.cpp test_main.cpp)

# 链接库
target_link_libraries(lexer_test 
    compiler_lib 
    GTest::GTest 
    GTest::Main
)

# 添加测试
add_test(NAME LexserTests COMMAND lexer_test)