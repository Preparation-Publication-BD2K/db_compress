#include "base.h"
#include "utility.h"

#include <cmath>
#include <vector>
#include <iostream>

namespace db_compress {

void TestDynamicList() {
    std::vector<size_t> index;
    index.push_back(0); index.push_back(1); index.push_back(2);
    std::vector<size_t> cap;
    cap.push_back(1); cap.push_back(2); cap.push_back(3);
    DynamicList<int> dynamic_list(cap);
    dynamic_list[index] = 1;
    index[2] = 0;
    dynamic_list[index] = 2;
    if (dynamic_list.size() != 6)
        std::cerr << "Dynamic List Unit Test Failed!\n";
    if (dynamic_list[index] != 2 || dynamic_list[3] != 2)
        std::cerr << "Dynamic List Unit Test Failed!\n";
    const DynamicList<int>& another = dynamic_list;
    if (another[index] != 2)
        std::cerr << "Dynamic List Unit Test Failed!\n";
    index[2] = 2;
    if (dynamic_list[index] != 1)
        std::cerr << "Dynamic List Unit Test Failed!\n";
    dynamic_list[index] = 3;
    if (dynamic_list.size() != 6 || another[index] != 3)
        std::cerr << "Dynamic List Unit Test Failed!\n";   
}

void TestQuantization() {
    std::vector<int> cnt;
    std::vector<Prob> prob;
    cnt.push_back(5);
    cnt.push_back(2);
    cnt.push_back(1);
    cnt.push_back(1);
    cnt.push_back(0);
    Quantization(&prob, cnt, 3);
    if (prob[0] != GetProb(3, 3) || prob[1] != GetProb(5, 3) || 
        prob[2] != GetProb(6, 3) || prob[3] != GetProb(7, 3) )
        std::cerr << "Quantization Unit Test Failed!\n";
}

void TestPIProduct() {
    ProbInterval PI(GetProb(3, 3), GetProb(5, 3));
    std::vector<unsigned char> emit_bytes;
    PI = GetPIProduct(PI, ProbInterval(GetProb(1, 2), GetProb(3, 2)), &emit_bytes);
    if (PI.l != GetProb(14, 5) || PI.r != GetProb(18, 5) || emit_bytes.size() != 0)
        std::cerr << "PIProduct Unit Test Failed!\n";
    PI = GetPIProduct(PI, ProbInterval(GetProb(1, 13), GetProb(2, 13)), NULL);
    if (PI.l != GetProb(14 * 8192 + 4, 18) || PI.r != GetProb(14 * 8192 + 8, 18))
        std::cerr << "PIProduct Unit Test Failed!\n";
    PI = GetPIProduct(PI, ProbInterval(GetZeroProb(), GetOneProb()), &emit_bytes);
    if (emit_bytes.size() != 2 || emit_bytes[0] != 0x70 || emit_bytes[1] != 0x01 ||
        PI.l != GetZeroProb() || PI.r != GetOneProb())
        std::cerr << "PIProduct Unit Test Failed!\n";
    UnitProbInterval PIb(14 * 2048 + 1, 16);
    ReducePI(&PIb, emit_bytes);
    if (PIb.num != 0 || PIb.exp != 0)
        std::cerr << "PIProduct Unit Test Failed!\n";
    ProbInterval PIt(GetProb(65535, 17), GetProb(65538, 17));
    PI = GetPIProduct(PI, PIt, &emit_bytes);
    if (PI.l != GetZeroProb() || PI.r != GetOneProb() || 
        emit_bytes.size() != 4 || emit_bytes[2] != 0x80 || emit_bytes[3] != 0)
        std::cerr << "PIProduct Unit Test Failed!\n";
}

void TestFloatQuantization() {
    unsigned char bytes[4];
    ConvertSinglePrecision(0.1, bytes);
    if (bytes[0] != 0x3d || bytes[1] != 0xcc || bytes[2] != 0xcc || bytes[3] != 0xcd)
        std::cerr << "Float Quantization Unit Test Failed!\n";
    if (fabs(ConvertSinglePrecision(bytes) - 0.1) > 1e-6)
        std::cerr << "Float Quantization Unit Test Failed!\n";
}

void TestBitString() {
    if (GetByte(0x7fdecba0, 4) != 0xfd) 
        std::cerr << "Bit String Unit Test Failed!\n";
    BitString str;
    str.Clear();
    StrCat(&str, 0xff);
    if (str.length != 8 || str.bits[0] != 0xff000000)
        std::cerr << "Bit String Unit Test Failed!\n";
    StrCat(&str, 0x7fec, 12);
    if (str.length != 20 || str.bits[0] != 0xfffec000)
        std::cerr << "Bit String Unit Test Failed!\n";
    BitString copy(str);
    StrCat(&str, copy);
    if (str.length != 40 || str.bits.size() != 2 || 
        str.bits[0] != 0xfffecfff || str.bits[1] != 0xec000000)
        std::cerr << "Bit String Unit Test Failed!\n";
    if (ComputePrefix(str, 12) != 0xfff)
        std::cerr << "Bit String Unit Test Failed!\n";
    GetBitStringFromProbInterval(&str, ProbInterval(GetProb(7, 7), GetProb(9, 7)) );
    if (str.length != 7 || str.bits[0] != 0x10000000)
        std::cerr << "Bit String Unit Test Failed!\n";
}

void Test() {
    TestDynamicList();
    TestQuantization();
    TestPIProduct();
    TestFloatQuantization();
    TestBitString();
}

}  // namespace db_compress

int main() {
    db_compress::Test();
}
