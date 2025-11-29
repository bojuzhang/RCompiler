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
add_test(NAME LexerTests COMMAND lexer_test)

# 添加测试可执行文件
add_executable(parser_test parser_test.cpp test_main.cpp)

# 链接库
target_link_libraries(parser_test 
    compiler_lib 
    GTest::GTest 
    GTest::Main
)

# 添加测试
add_test(NAME ParserTests COMMAND parser_test)

# 添加 IRBuilder 测试可执行文件
add_executable(irbuilder_test irbuilder_test.cpp)

# 链接库
target_link_libraries(irbuilder_test
    compiler_lib
    GTest::GTest
    GTest::Main
)

# 添加测试
add_test(NAME IRBuilderTests COMMAND irbuilder_test)

# 添加 TypeMapper 测试可执行文件
add_executable(typemapper_test typemapper_test.cpp)

# 链接库
target_link_libraries(typemapper_test
    compiler_lib
    GTest::GTest
    GTest::Main
)

# 添加测试
add_test(NAME TypeMapperTests COMMAND typemapper_test)

# 添加 BuiltinDeclarator 测试可执行文件
add_executable(builtindeclarator_test builtindeclarator_test.cpp)

# 链接库
target_link_libraries(builtindeclarator_test
    compiler_lib
    GTest::GTest
    GTest::Main
)

# 添加测试
add_test(NAME BuiltinDeclaratorTests COMMAND builtindeclarator_test)

# 添加 ExpressionGenerator 测试可执行文件
add_executable(expressiongenerator_test expressiongenerator_test.cpp)

# 链接库
target_link_libraries(expressiongenerator_test
    compiler_lib
    GTest::GTest
    GTest::Main
)

# 添加测试
add_test(NAME ExpressionGeneratorTests COMMAND expressiongenerator_test)

# 添加 StatementGenerator 测试可执行文件
add_executable(statementgenerator_test statementgenerator_test.cpp)

# 链接库
target_link_libraries(statementgenerator_test
    compiler_lib
    GTest::GTest
    GTest::Main
)

# 添加测试
add_test(NAME StatementGeneratorTests COMMAND statementgenerator_test)

# 添加 FunctionCodegen 测试可执行文件
add_executable(functioncodegen_test functioncodegen_test.cpp)

# 链接库
target_link_libraries(functioncodegen_test
    compiler_lib
    GTest::GTest
    GTest::Main
)

# 添加测试
add_test(NAME FunctionCodegenTests COMMAND functioncodegen_test)

# 添加 IRGenerator 测试可执行文件
add_executable(irgenerator_test irgenerator_test.cpp test_main.cpp)

# 链接库
target_link_libraries(irgenerator_test
    compiler_lib
    GTest::GTest
    GTest::Main
)

# 添加测试
add_test(NAME IRGeneratorTests COMMAND irgenerator_test)