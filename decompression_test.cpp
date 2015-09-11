#include "attribute.h"
#include "base.h"
#include "data_io.h"
#include "model.h"
#include "compression.h"
#include "decompression.h"

#include <cmath>
#include <vector>
#include <memory>
#include <iostream>

namespace db_compress {

void RegisterAttributes() {
    RegisterAttrValueCreator(0, new EnumAttrValueCreator(), BASE_TYPE_ENUM);
    RegisterAttrValueCreator(1, new IntegerAttrValueCreator(), BASE_TYPE_INTEGER);
    RegisterAttrValueCreator(2, new StringAttrValueCreator(), BASE_TYPE_STRING);
}

void TestCategoricalDB() {
    std::vector<int> schema_;
    schema_.push_back(0); schema_.push_back(0);
    Schema schema(schema_);
    CompressionConfig config;
    config.allowed_err.push_back(0);
    config.allowed_err.push_back(0);
    config.sort_by_attr = -1;
    {
        Compressor compressor("compression_test.txt", schema, config);
        while (compressor.RequireMoreIterations()) {
            for (int i = 0; i < 1000; ++i) {
                Tuple tuple(2);
                TupleIStream istream(&tuple, schema);
                istream << (size_t)i / 500 << (size_t)i / 500;
                compressor.ReadTuple(tuple);
            }
            compressor.EndOfData();
        }
    }

    {
        Decompressor decompressor("compression_test.txt", schema);
        decompressor.Init();
        for (int i = 0; i < 1000; ++i) {
            Tuple tuple(2);
            if (!decompressor.HasNext())
                std::cerr << "CategoricalDB Decompression Test Failed!\n";
            decompressor.ReadNextTuple(&tuple);
            TupleOStream ostream(tuple, schema);
            size_t a, b;
            ostream >> a >> b;
            if ((int)a != i / 500 || (int)b != i / 500)
                std::cerr << "CategoricalDB Decompression Test Failed!\n";
        }
        if (decompressor.HasNext())
            std::cerr << "CategoricalDB Decompression Test Failed!\n";
    }
}

void TestNumericalDB() {
}

void TestStringDB() {
}

void Test() {
    RegisterAttributes();
    TestCategoricalDB();
    TestNumericalDB();
    TestStringDB();
}

}  // namespace db_compress

int main() {
    db_compress::Test();
}
