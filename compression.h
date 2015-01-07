#ifndef COMPRESSION_H
#define COMPRESSION_H

#include "base.h"
#include "data_io.h"
#include "model.h"

#include <vector>
#include <memory>

namespace db_compress {
   
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
