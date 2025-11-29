#include <gtest/gtest.h>

// 声明外部测试函数
extern void runIRGeneratorTests();

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    // 运行我们的自定义测试
    runIRGeneratorTests();
    
    return RUN_ALL_TESTS();
}