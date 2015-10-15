#include "base.h"
#include "model.h"
#include "compression.h"
#include "decompression.h"
#include "unit_test.h"

#include <vector>
#include <iostream>

namespace db_compress {

Schema schema;
CompressionConfig config;

void PrepareData() {
    std::vector<int> schema_;
    for (int i = 0; i < 10; ++i) {
        RegisterAttrModel(i, new MockModelCreator(i + 1));
        schema_.push_back(i);
    }
    schema = Schema(schema_);
    config.allowed_err = std::vector<double>(10, 0);
    config.sort_by_attr = -1;
}

void TestRun() {
    int value[] = {0,1,1,2,1,4,6,0,3,3};
    std::vector<MockAttr> vec;
    Tuple tuple(10);
    for (int i = 0; i < 10; ++i)
        vec.push_back(MockAttr(value[i]));
    for (int i = 0; i < 10; ++i)
        tuple.attr[i] = &vec[i];

    {
        Compressor compressor("compression_test.txt", schema, config);
        while (compressor.RequireMoreIterations()) {
            compressor.ReadTuple(tuple);
            compressor.EndOfData();
        }
    }

    {
        Decompressor decompressor("compression_test.txt", schema);
        decompressor.Init();
        Tuple tuple_(10);
        decompressor.ReadNextTuple(&tuple_);
        for (int i = 0; i < 10; i++)
        if (static_cast<const MockAttr*>(tuple_.attr[i])->Val() != value[i])
            std::cerr << "Test Run Failed!\n";
    }
}

void Test() {
    PrepareData();
    TestRun();
}

}  // namespace db_compress

int main() {
    db_compress::Test();
}
