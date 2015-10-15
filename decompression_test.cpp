#include "base.h"
#include "model.h"
#include "decompression.h"
#include "unit_test.h"

#include <vector>
#include <iostream>

namespace db_compress {

Schema schema;

void PrepareData() {
    RegisterAttrModel(0, new MockModelCreator(2));
    std::vector<int> schema_; schema_.push_back(0); schema_.push_back(0);
    schema = Schema(schema_);
    std::ofstream fout("compression_test.txt");
    unsigned char data[] = {1,0,0,0,1,0,2,0,2,0x5c};
    for (int i = 0; i < 10; ++i )
        fout << data[i];
    fout.close();
}

void TestDecompression() {
    Decompressor decompressor("compression_test.txt", schema);
    decompressor.Init();
    for (int i = 0; i < 2; ++i) {
        Tuple tuple(2);
        if (!decompressor.HasNext())
            std::cerr << "Decompression Unit Test Failed!\n";
        decompressor.ReadNextTuple(&tuple);
        if (static_cast<const MockAttr*>(tuple.attr[0])->Val() != 0 ||
            static_cast<const MockAttr*>(tuple.attr[1])->Val() != 1)
            std::cerr << "Decompression Unit Test Failed!\n";
    }
    if (decompressor.HasNext())
        std::cerr << "Decompression Unit Test Failed!\n";
}

void Test() {
    PrepareData();
    TestDecompression();
}

}  // namespace db_compress

int main() {
    db_compress::Test();
}
