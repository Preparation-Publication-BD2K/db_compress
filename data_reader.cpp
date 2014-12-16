#include "compression.h"
#include "data_reader.h"
#include <vector>
#include <string>

namespace db_compress {

TupleReader::TupleReader(Schema* schema, DataReader* data_reader) : 
    schema_(schema),
    data_reader_(data_reader) {}

Tuple* TupleReader::ReadTuple() {
    Tuple* ret = new Tuple(schema_->cols_);
    vector<string> parsed_strings;
    data_reader_->ReadTupleString(&parsed_strings);
    for (int i = 0; i < schema->cols_; ++i) {
        ret->attr_[i] = GetAttrValue(schema_->attr_type_[i], parsed_strings[i]);
    }
    return ret;
}

}  // namespace db_compress
