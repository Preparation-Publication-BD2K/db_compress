#ifndef DATA_IO_H
#define DATA_IO_H

#include "base.h"
#include <vector>

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

}

#endif
