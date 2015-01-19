#ifndef DATA_IO_H
#define DATA_IO_H

#include "base.h"
#include <vector>
#include <iostream>
#include <fstream>

namespace db_compress {

class TupleIStream {
  private:
    Tuple* tuple_;
    const Schema& schema_;
    int index_;
  public:
    TupleIStream(Tuple* tuple, const Schema& schema);
    friend TupleIStream& operator<<(TupleIStream& stream, int target);
    friend TupleIStream& operator<<(TupleIStream& stream, double target);
    friend TupleIStream& operator<<(TupleIStream& stream, const std::string& target);
};

TupleIStream& operator<<(TupleIStream& stream, int target);
TupleIStream& operator<<(TupleIStream& stream, double target);
TupleIStream& operator<<(TupleIStream& stream, const std::string& target); 

class TupleOStream {
  private:
    const Tuple& tuple_;
    const Schema& schema_;
    int index_;
  public:
    TupleOStream(const Tuple& tuple, const Schema& schema);
    friend TupleOStream& operator>>(TupleOStream& stream, int& target);
    friend TupleOStream& operator>>(TupleOStream& stream, double& target);
    friend TupleOStream& operator>>(TupleOStream& stream, std::string& target);
};

TupleOStream& operator>>(TupleOStream& stream, int& target);
TupleOStream& operator>>(TupleOStream& stream, double& target);
TupleOStream& operator>>(TupleOStream& stream, std::string& target); 

struct CompressionConfig {
    std::vector<double> allowed_err;
    int sort_by_attr;
};

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
};

}  // namespace db_compress

#endif
