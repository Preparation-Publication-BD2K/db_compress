#include "attribute.h"
#include "base.h"
#include "compression.h"
#include "data_io.h"
#include "model.h"
#include <vector>
#include <iostream>

namespace db_compress {

namespace {

Schema schema;
CompressionConfig config;
std::vector<std::unique_ptr<Tuple>> tuple;

void CreateTuple(Tuple* tuple, size_t a, size_t b) {
    TupleIStream istream(tuple, schema);
    istream << a << b;
}

void PrepareDB() {
    RegisterAttrValueCreator(0, new EnumAttrValueCreator(), BASE_TYPE_ENUM);
    std::vector<int> schema_; schema_.push_back(0); schema_.push_back(0); 
    schema = Schema(schema_);
    for (int i = 0; i < 10000; ++ i) {
        std::unique_ptr<Tuple> ptr(new Tuple(2));
        tuple.push_back(std::move(ptr));
    }
    for (int i = 0; i < 5000; ++ i) 
        CreateTuple(tuple[i].get(), 0, 0);
    for (int i = 5000; i < 8000; ++ i)
        CreateTuple(tuple[i].get(), 0, 1);
    for (int i = 8000; i < 10000; ++ i)
        CreateTuple(tuple[i].get(), 1, 1);
    config.allowed_err.push_back(0.01);
    config.allowed_err.push_back(0.01);
    config.sort_by_attr = -1;
}

void Compress() {
    Compressor compressor("compression_test.txt", schema, config);
    while (1) {
        for (size_t i = 0; i < 10000; ++ i) {
            compressor.ReadTuple(*tuple[i]);
        }
        compressor.EndOfData();
        if (!compressor.RequireMoreIterations())
            break;
    }
}

}  // anonymous namespace

void TestCompression() {
    Compress();
    std::ifstream fin("compression_test.txt");
    
    char c;
    std::vector<unsigned char> file;
    while (fin.get(c)) {
        file.push_back((unsigned char)c);
    }
    if (file[0] != 14)
        std::cerr << "Compression Test Failed!\n";
    if (file[1] != 0 || file[2] != 1)
        std::cerr << "Compression Test Failed!\n";
    if (file[3] != 0 || file[4] != 0 || file[5] != 8 || file[6] != 204)
        std::cerr << "Compression Test Failed!\n";
    if (file[7] != 0 || file[8] != 1 || file[9] != 8 || file[10] != 0 || file[11] != 2 || 
        file[12] != 159 || file[13] != 0)
        std::cerr << "Compression Test Failed!\n";
    for (int i = 14; i < 12 + 625; i++ )
        if (file[i] != 0) {
            std::cerr << "Compression Test Failed!\n";
            break;
        }
    for (int i = 639; i < 637 + 1024; i++ )
        if (file[i] != 0xff) {
            std::cerr << "Compression Test Failed!\n";
            break;
        }
    for (int i = 1663; i < 1661 + 375; i++ )
        if (file[i] != 0) {
            std::cerr << "Compression Test Failed!\n";
            break;
        }
    for (int i = 2038; i < 2036 + 768; i++ )
        if (file[i] != 0xff) {
            std::cerr << "Compression Test Failed!\n";
            break;
        }
    for (int i = 2806; i < 2804 + 250; i++ )
        if (file[i] != 0) {
            std::cerr << "Compression Test Failed!\n";
            break;
        }
    for (int i = 3056; i < 3054 + 256; i++ )
        if (file[i] != 0xff) {
            std::cerr << "Compression Test Failed!\n";
            break;
        }
    if (file.size() != 3312)
        std::cerr << "Compression Test Failed!\n";
}

void Test() {
    PrepareDB();
    TestCompression();
}

}  // namespace db_compress

int main() {
    db_compress::Test();
}
