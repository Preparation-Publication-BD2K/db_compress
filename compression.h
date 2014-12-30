#ifndef COMPRESSION_H
#define COMPRESSION_H

#include "base.h"
#include "data_io.h"
#include "model.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <memory>

namespace db_compress {

class ByteWriter {
  private:
    std::vector<char> block_unwritten_prefix_;
    std::vector<char> block_unwritten_suffix_;
    std::vector<int> block_pos_;
    std::ofstream file_;
    std::vector<int> bit_str_len_;
  public:
    ByteWriter(std::vector<int>* block_length, const std::string& file_name);
    void WriteByte(char byte, int block);
    // Only write the least significant (len) bits
    void WriteLess(char byte, int len, int block);
    void WriteRemaining();
};
   
class Compressor {
  private:
    std::string outputFile_;
    Schema schema_;
    std::unique_ptr<ModelLearner> learner_;
    std::vector< std::unique_ptr<Model> > model_;
    std::vector<int> attr_order_;
    std::unique_ptr<ByteWriter> byte_writer_;
    int stage_;
    int num_of_tuples_;
    int implicit_prefix_length_;
    std::vector<int> block_length_;
  public:
    // sort_by_attr_index_ equals -1 meaning no specific sorting required
    Compressor(char* outputFile, const Schema& schema, const CompressionConfig& config);
    void ReadTuple(const Tuple& tuple);
    bool RequireMoreIteration() const;
    void EndOfData();
};

}  // namespace db_compress

#endif
