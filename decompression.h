#ifndef DECOMPRESSION_H
#define DECOMPRESSION_H

#include "base.h"
#include "data_io.h"
#include <string>

namespace db_compress {

class Decompressor {
  public:
    Decompressor(char* compressedFileName, Schema* schema);    
};

}  // namespace db_compress

#endif
