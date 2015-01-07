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
    Schema schema_;
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
  public:
    TupleOStream(const Tuple* tuple, const Schema& schema);
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
    std::vector<char> block_unwritten_prefix_;
    std::vector<char> block_unwritten_suffix_;
    std::vector<int> block_pos_;
    std::ofstream file_;
    std::vector<int> bit_str_len_;
  public:
    ByteWriter(std::vector<int>* block_length, const std::string& file_name);
    ~ByteWriter();
    void WriteByte(char byte, int block);
    // Only write the least significant (len) bits
    void WriteLess(char byte, int len, int block);
};

}

#endif
