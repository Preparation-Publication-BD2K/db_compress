#include "data_io.h"
#include "attribute.h"
#include <iostream>
#include <fstream>

namespace db_compress {

void TestTupleStream() {
    RegisterAttrValueCreator(0, new IntegerAttrValueCreator(), BASE_TYPE_INTEGER);
    std::vector<int> schema_; schema_.push_back(0); schema_.push_back(0);
    Schema schema(schema_);
    Tuple tuple(2);
    TupleIStream istream(&tuple, schema);
    istream << 0 << 1;
    TupleOStream ostream(tuple, schema);
    int a, b;
    ostream >> a >> b;
    if (a != 0 || b != 1)
        std::cerr << "Tuple Stream Unit Test Failed!\n";
}

void TestByteWriter() {
    {
        std::vector<int> blocks;
        blocks.push_back(3);
        blocks.push_back(7);
        blocks.push_back(14);
        ByteWriter writer(&blocks, "byte_writer_test.txt");
        // We will write string abc (0x61, 0x62, 0x63)
        // As bit string : 01100001, 01100010, 01100011
        writer.WriteLess(3, 3, 0);
        writer.WriteLess(5, 7, 1);
        writer.WriteByte(0x89, 2);
        writer.WriteLess(0x23, 6, 2);
    }
    std::ifstream fin("byte_writer_test.txt");
    std::string str;
    fin >> str;
    if (str != "abc") {
        std::cerr << "Read in string " << str << "!\n";
        std::cerr << "ByteWriter Unit Test Failed!\n";
    }
}

void Test() {
    TestTupleStream();
    TestByteWriter();
}

}  // namespace db_compress

int main() {
    db_compress::Test();
}
