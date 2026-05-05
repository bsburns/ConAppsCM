#include <iostream>
#include "magic_enum.hpp"

extern void test_func();

enum class TestEnum : int {
    TE_ONE,
    TE_TWO
};

int main() {
    TestEnum te = TestEnum::TE_TWO;
    std::cout << "Hello, CMake on Linux v20  - 2!" << std::endl;
    test_func();
    std::cout << "te=" << std::string(magic_enum::enum_name(te)) << std::endl;
    return 0;
}
