#include "base.h"
#include "data_io.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <memory>

namespace db_compress {

void TestByteWriter() {
    {
        std::vector<size_t> blocks;
        blocks.push_back(3);
        blocks.push_back(7);
        blocks.push_back(14);
        blocks.push_back(6);
        blocks.push_back(17);
        ByteWriter writer(&blocks, "byte_writer_test.txt");
        // We will write string abcdef (0x61, 0x62, 0x63, 0x64, 0x65, 0x66)
        // As bit string : 01100001, 01100010, 01100011, 01100100
        writer.WriteLess(3, 3, 0);
        writer.WriteLess(5, 7, 1);
        writer.WriteByte(0x89, 2);
        writer.WriteLess(0x23, 6, 2);
        writer.WriteLess(0x19, 6, 3);
        writer.WriteLess(0, 1, 4);
        writer.Write16Bit(0x32b3, 4);
    }
    std::ifstream fin("byte_writer_test.txt");
    std::string str;
    fin >> str;
    if (str != "abcdef") {
        std::cerr << "ByteWriter Unit Test Failed!\n";
    }
}

void TestByteReader() {
    {
        std::vector<size_t> blocks;
        blocks.push_back(84);
        ByteWriter writer(&blocks, "byte_writer_test.txt");
        // We will write bit string 0x123456789abcdef012345
        writer.WriteByte(0x12, 0);
        writer.WriteByte(0x34, 0);
        writer.WriteByte(0x56, 0);
        writer.WriteByte(0x78, 0);
        writer.WriteByte(0x9a, 0);
        writer.WriteByte(0xbc, 0);
        writer.WriteByte(0xde, 0);
        writer.WriteByte(0xf0, 0);
        writer.WriteByte(0x12, 0);
        writer.WriteByte(0x34, 0);
        writer.WriteLess(5, 4, 0);
    }
    ByteReader reader("byte_writer_test.txt");
    if (reader.ReadByte() != 0x12)
        std::cerr << "Byte Reader Unit Test Failed!\n";
    if (reader.Read16Bit() != 0x3456)
        std::cerr << "Byte Reader Unit Test Failed!\n";
    bool bit[4];
    for (int i = 0; i < 4; ++ i)
        bit[i] = reader.ReadBit();
    if (bit[0] != 0 || bit[1] != 1 || bit[2] != 1 || bit[3] != 1)
        std::cerr << "Byte Reader Unit Test Failed!\n";
    if (reader.ReadByte() != 0x89)
        std::cerr << "Byte Reader Unit Test Failed!\n";
    if (reader.Read16Bit() != 0xabcd)
        std::cerr << "Byte Reader Unit Test Failed!\n";
    unsigned char bytes[4];
    reader.Read32Bit(bytes);
    if (bytes[0] != 0xef || bytes[1] != 0x01 || bytes[2] != 0x23 || bytes[3] != 0x45)
        std::cerr << "Byte Reader Unit Test Failed!\n";
}

void Test() {
    TestByteWriter();
    TestByteReader();
}

}  // namespace db_compress

int main() {
    db_compress::Test();
}
