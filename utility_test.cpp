#include "utility.h"
#include <vector>
#include <iostream>

namespace db_compress {

void TestDynamicList() {
    std::vector<size_t> index;
    index.push_back(0); index.push_back(1); index.push_back(2);
    DynamicList<int> dynamic_list;
    dynamic_list[index] = 1;
    index[2] = 3;
    dynamic_list[index] = 2;
    if (dynamic_list.size() != 2)
        std::cerr << "Dynamic List Unit Test Failed!\n";
    if (dynamic_list[index] != 2 || dynamic_list[1] != 2)
        std::cerr << "Dynamic List Unit Test Failed!\n";
    const DynamicList<int>& another = dynamic_list;
    if (another[index] != 2)
        std::cerr << "Dynamic List Unit Test Failed!\n";
    index[2] = 2;
    if (dynamic_list[index] != 1)
        std::cerr << "Dynamic List Unit Test Failed!\n";
    dynamic_list[index] = 3;
    if (dynamic_list.size() != 2 || another[index] != 3)
        std::cerr << "Dynamic List Unit Test Failed!\n";   
}


void Test() {
    TestDynamicList();
}

}  // namespace db_compress

int main() {
    db_compress::Test();
}
