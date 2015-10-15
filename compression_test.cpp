#include "base.h"
#include "compression.h"
#include "data_io.h"
#include "model.h"
#include "unit_test.h"

#include <vector>
#include <iostream>

namespace db_compress {

Schema schema;
CompressionConfig config;
Tuple tuple(2);

void PrepareData() {
    RegisterAttrModel(0, new MockModelCreator(2));
    std::vector<int> schema_; schema_.push_back(0); schema_.push_back(0); 
    schema = Schema(schema_);
    config.allowed_err.push_back(0.01);
    config.allowed_err.push_back(0.01);
    config.sort_by_attr = -1;
    tuple.attr[0] = new MockAttr(0);
    tuple.attr[1] = new MockAttr(1);
}

void TestCompression() {
    Compressor compressor("compression_test.txt", schema, config);
    while (1) {
        compressor.ReadTuple(tuple);
        compressor.ReadTuple(tuple);
        compressor.EndOfData();
        if (!compressor.RequireMoreIterations())
            break;
    }

    std::ifstream fin("compression_test.txt");
    char c;
    std::vector<unsigned char> file;
    while (fin.get(c)) {
        file.push_back((unsigned char) c);
    }
    unsigned char correct_answer[] = {1, 0, 0, 0, 1, 0, 2, 0, 2, 0x5c};
    if (file.size() != 10)
        std::cerr << "Compression Unit Test Failed!\n";
    for (int i = 0; i < 10; ++i)
    if (file[i] != correct_answer[i])
        std::cerr << "Compression Unit Test Failed!\n";
}

void Test() {
    PrepareData();
    TestCompression();
}

}  // namespace db_compress

int main() {
    db_compress::Test();
}
