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

TupleIStream& operator<<(TupleIStream& tuple_stream, int val) {
    AttrValueCreator* creator = GetAttrValueCreator(tuple_stream.schema_.attr_type[tuple_stream.index_]);
    if ((tuple_stream.tuple_->attr[tuple_stream.index_ ++] = creator->GetAttrValue(val)) == NULL) {
        std::cerr << "Error while reading tuple attr\n";
    }
    return tuple_stream;
}

TupleOStream::TupleOStream(const Tuple& tuple, const Schema& schema) :
    tuple_(tuple),
    schema_(schema),
    index_(0) {
}

TupleOStream& operator>>(TupleOStream& tuple_stream, int& val) {
    AttrValueCreator* creator = GetAttrValueCreator(tuple_stream.schema_.attr_type[tuple_stream.index_]);
    creator->ReadAttrValue(*tuple_stream.tuple_.attr[tuple_stream.index_ ++], &val);
    return tuple_stream;
}
//Todo: complete all other << and >> operators.

/*
 * Since the prefix/suffix has at most 7 bits, we can use 0xff as special symbol to indicate
 * that the prefix/suffix is not used yet.
 */
ByteWriter::ByteWriter(std::vector<int>* block_length, const std::string& file_name) :
    block_unwritten_prefix_(block_length->size()),
    block_unwritten_suffix_(block_length->size(), 0xff),
    file_(file_name) {
    block_pos_.swap(*block_length);
    int total_len = 0;
    for (int i = 0; i < block_pos_.size(); i++) {
        int current_block_len = block_pos_[i];
        block_pos_[i] = total_len;
        total_len += current_block_len;
    }
    for (int i = 0; i < block_pos_.size(); i++) {
        if ((block_pos_[i] & 7) == 0)
            block_unwritten_suffix_[i] = 0;
    }
}

// Write all the remaining, we can use block_pos_ to identify the prefix/suffix length
// of corresponding blocks
ByteWriter::~ByteWriter() {
    for (int i = 1; i < block_pos_.size(); i++ )
    if ((block_pos_[i - 1] & 7) != 0) {
        int suffix_length = (block_pos_[i - 1] & 7);
        if (block_pos_[i] < block_pos_[i - 1] + (8 - suffix_length)) {
            int len = block_pos_[i] - block_pos_[i - 1];
            block_unwritten_suffix_[i] = 
                (block_unwritten_suffix_[i - 1] << len)
                | (block_unwritten_prefix_[i]);
        } else {
            file_.seekp(block_pos_[i - 1] >> 3, std::ios_base::beg);
            unsigned char prefix = block_unwritten_prefix_[i];
            unsigned char suffix = block_unwritten_suffix_[i - 1];
            suffix <<= 8 - suffix_length;
            file_.put(suffix | prefix);
        }
    }
}

void ByteWriter::WriteByte(unsigned char byte, int block) {
    WriteLess(byte, 8, block);
}

void ByteWriter::WriteLess(unsigned char byte, int len, int block) {
    file_.seekp(block_pos_[block] >> 3, std::ios_base::beg);
    byte = (byte & ((1 << len) - 1)); 
    if (block_unwritten_suffix_[block] != 0xff) {
        unsigned char suffix = block_unwritten_suffix_[block];
        int needed_len = 8 - (block_pos_[block] & 7);
        if (needed_len <= len) {
            len -= needed_len;
            file_.put((suffix << needed_len) | (byte >> len));
            byte = byte & ((1 << len) - 1);
            block_unwritten_suffix_[block] = byte;
            block_pos_[block] += needed_len + len;
        } else {
            block_unwritten_suffix_[block] = ((block_unwritten_suffix_[block] << len) | byte);
            block_pos_[block] += len;
        }
    }
    else {
        int needed_len = 8 - (block_pos_[block] & 7);
        if (needed_len <= len) {
            len -= needed_len;
            unsigned char pad = (byte >> len);
            block_unwritten_prefix_[block] = ((block_unwritten_prefix_[block] << needed_len) | pad);
            byte = byte & ((1 << len) - 1);
            block_unwritten_suffix_[block] = byte;
            block_pos_[block] += needed_len + len;
        } else {
            block_unwritten_prefix_[block] = ((block_unwritten_prefix_[block] << len) | byte);
            block_pos_[block] += len;
        }
    }
}

}  // namespace db_compress
