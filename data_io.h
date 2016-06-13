#ifndef DATA_IO_H
#define DATA_IO_H

#include "base.h"

#include <vector>
#include <iostream>
#include <fstream>

namespace db_compress {

/*
 * ByteWriter is a utility class that can be used to write bit strings.
 * The constructor takes a vector of integers indicating the (predetermined)
 * block lengths. Then the class provide interface to continue writing bit 
 * strings to any of these blocks in arbitrary order. Note that only if the
 * object is destoryed will the data be completely written into the file, 
 * otherwise some of the data might be held in memory buffer.
 */
class ByteWriter {
  private:
    // Since we can only write byte as a unit, sometimes we have unwritten
    // prefix/suffix bis string that is shorter than 8 bits, we store them
    // in the buffer.
    std::vector<unsigned char> block_unwritten_prefix_;
    std::vector<unsigned char> block_unwritten_suffix_;
    // The starting point of incoming bit stream for each block
    std::vector<size_t> block_pos_;
    std::ofstream file_;
  public:
    ByteWriter(std::vector<size_t>* block_length, const std::string& file_name);
    ~ByteWriter();
    void WriteByte(unsigned char byte, size_t block);
    // Only write the least significant (len) bits
    void WriteLess(unsigned char byte, size_t len, size_t block);
    // Write 16 bits at once
    void Write16Bit(unsigned int val, size_t block);
    // Write 32 bits at once
    void Write32Bit(unsigned char byte[4], size_t block);
};

/* 
 * ByteReader is a utility class that can be used to read bit strings.
 * It allows us to read in single bit at each time.
 */
class ByteReader {
  private:
    std::ifstream fin_;
    unsigned int buffer_, buffer_len_;
  public:
    ByteReader(const std::string& file_name);
    ~ByteReader();
    unsigned char ReadByte();
    bool ReadBit();
    unsigned int Read16Bit();
    void Read32Bit(unsigned char* bytes);
};

}  // namespace db_compress

#endif
