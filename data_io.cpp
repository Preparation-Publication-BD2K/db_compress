#include "compression.h"
#include "data_io.h"
#include "attribute.h"
#include <iostream>
#include <vector>
#include <string>

namespace db_compress {

TupleIStream::TupleIStream(Tuple* tuple, const Schema& schema) :
    tuple_(tuple),
    schema_(schema),
    index_(0) {
}

TupleIStream& operator<<(TupleIStream& tuple_stream, const std::string& str) {
    AttrValueCreator* creator = GetAttrValueCreator(tuple_stream.schema_.attr_type[tuple_stream.index_]);
    if ((tuple_stream.tuple_->attr[tuple_stream.index_ ++] = creator->GetAttrValue(str)) == NULL) {
        std::cerr << "Error while reading tuple attr\n";
    }
    return tuple_stream;
}

//Todo: complete all other << and >> operators.

}  // namespace db_compress
