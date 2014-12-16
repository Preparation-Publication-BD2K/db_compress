#ifndef DATA_READER_H
#define DATA_READER_H

#include "compression.h"

namespace db_compress {

virtual class DataReader {
  public:
    virtual void ReadTupleString(vector<string> *vec);
};

class TupleReader {
  public:
    TupleReader(Schema*, DataReader*);
    // Caller takes the ownership of Tuple.
    Tuple* ReadTuple();
  private:
    Schema* schema_;
    DataReader* data_reader_;
};

}

#endif
