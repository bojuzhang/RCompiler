#include <gtest/gtest.h>

// 声明外部测试函数
extern void runIRGeneratorTests();

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    return RUN_ALL_TESTS();
}