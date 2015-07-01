#include "base.h"
#include "utility.h"

#include <cmath>
#include <vector>
#include <iostream>

namespace db_compress {

void TestDynamicList() {
    std::vector<size_t> index;
    index.push_back(0); index.push_back(1); index.push_back(2);
    DynamicList<int> dynamic_list(3);
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

void TestQuantization() {
    std::vector<double> prob;
    std::vector<double> min_sep;
    min_sep.push_back(0.3);
    min_sep.push_back(0.2);
    min_sep.push_back(0.1);
    min_sep.push_back(0.1);
    prob.push_back(0.2);
    prob.push_back(0.4);
    prob.push_back(0.6);
    AdjustProbIntervals(&prob, min_sep);
    if (fabs(prob[0] - 0.3) > 0.01 || fabs(prob[1] - 0.5) > 0.01 || fabs(prob[2] - 0.666) > 0.01)
        std::cerr << "Quantization Unit Test Failed!\n";
    std::vector<double> cnt; 
    cnt.push_back(3);
    cnt.push_back(2);
    cnt.push_back(1);
    cnt.push_back(1);
    cnt.push_back(0);
    Quantization(&prob, cnt, 5);
    if (fabs(prob[0] - 0.4) > 0.01 || fabs(prob[1] - 0.6) > 0.01 || 
        fabs(prob[2] - 0.8) > 0.01 || fabs(prob[3] - 1) > 0.01)
        std::cerr << "Quantization Unit Test Failed!\n";
}

void TestReduceProbInterval() {
    ProbInterval PI(0.2, 0.6);
    std::vector<unsigned char> emit_bytes;
    PI = ReducePIProduct(PI, ProbInterval(0, 0.1), &emit_bytes);
    if (fabs(PI.l - 0.2) > 0.001 || fabs(PI.r - 0.24) > 0.001 || emit_bytes.size() != 0)
        std::cerr << "Reduce ProbInterval Unit Test Failed!\n";
    PI = ReducePIProduct(PI, ProbInterval(0.1, 0.15), NULL);
    if (fabs(PI.l - 0.204) > 0.001 || fabs(PI.r - 0.206) > 0.001)
        std::cerr << "Reduce ProbInterval Unit Test Failed!\n";
    PI = ReducePIProduct(PI, ProbInterval(0, 1), &emit_bytes);
    if (emit_bytes.size() != 1 || emit_bytes[0] != 52 || 
        fabs(PI.l - 0.224) > 0.001 || fabs(PI.r - 0.736) > 0.001)
        std::cerr << "Reduce ProbInterval Unit Test Failed!\n";
    ProbInterval PIt(0.2, 0.4), PIb(0.2, 0.3);
    ReducePI(&PIt, &PIb);
    if (PIt.l != 0.4 || PIt.r != 0.8 || PIb.l != 0.4 || PIb.r != 0.6)
        std::cerr << "Reduce ProbInterval Unit Test Failed!\n";
}

void TestLaplace() {
    if (fabs(GetMidValueFromExponential(2, 0, -1) - 1.386) > 0.001)
        std::cerr << "Laplace Unit Test Failed!\n";
    if (fabs(GetMidValueFromExponential(2, 0, 1000000) - 1.386) > 0.001)
        std::cerr << "Laplace Unit Test Failed!\n";
    double prob, value;
    GetPartitionPointFromExponential(2, 0, 1000000, 1, &prob, &value);
    if (value != 2 || fabs(prob - (1 - exp(-1))) > 0.001)
        std::cerr << "Laplace Unit Test Failed!\n";
    double result;
    std::vector<ProbInterval> vec;
    ProbInterval PI(0, 1);
    // Common Test    
    vec.clear();
    GetProbIntervalFromExponential(0.6, 0.5, 0.22, false, false, &result, &vec);
    PI = ReducePIProduct(vec, NULL);
    if (fabs(result - 0.66) > 0.02 || fabs(PI.l - 0.519) > 0.01 || fabs(PI.r - 0.769) > 0.01)
        std::cerr << "Laplace Unit Test Failed!\n";
    // Test Reversed Distribution
    vec.clear();
    GetProbIntervalFromExponential(0.6, 1, 0, true, true, &result, &vec);
    PI = ReducePIProduct(vec, NULL);
    if (fabs(result - 1) > 0.02 || fabs(PI.l - 0.035) > 0.01 || fabs(PI.r - 0.188) > 0.01)
        std::cerr << "Laplace Unit Test Failed!\n";
    // Failed Test From Actual Dataset
    GetProbIntervalFromExponential(6049.5, 288462, 10000, true, false,
                                   &result, &vec);
    if (vec.size() <= 1)
        std::cerr << "Laplace Unit Test Failed!\n";
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
    GetBitStringFromProbInterval(&str, ProbInterval(0.66, 0.68) );
    if (str.length != 7 || str.bits[0] != 0xaa000000)
        std::cerr << "Bit String Unit Test Failed!\n";
}

void Test() {
    TestDynamicList();
    TestQuantization();
    TestReduceProbInterval();
    TestLaplace();
    TestFloatQuantization();
    TestBitString();
}

}  // namespace db_compress

int main() {
    db_compress::Test();
}
