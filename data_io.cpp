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

ByteWriter::ByteWriter(std::vector<int>* block_length, const std::string& file_name) :
    block_unwritten_prefix_(block_length->size(), 1),
    block_unwritten_suffix_(block_length->size(), 0),
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
            block_unwritten_suffix_[i] = 1;
    }
    bit_str_len_.resize(256);
    bit_str_len_.shrink_to_fit();
    for (int i = 1; i <= 8; i++ )
        for (int j = (1 << (i - 1)); j < (1 << i); j++ )
            bit_str_len_[j] = i;
}

// Write all the remaining
ByteWriter::~ByteWriter() {
    for (int i = 1; i < block_pos_.size(); i++ )
    if (block_unwritten_prefix_[i] != 1) {
        file_.seekp(block_pos_[i - 1] >> 3, std::ios_base::beg);
        char prefix = block_unwritten_prefix_[i];
        prefix -= (1 << (bit_str_len_[prefix] - 1));
        char suffix = block_unwritten_suffix_[i - 1];
        suffix <<= 9 - bit_str_len_[suffix];
        file_.put(suffix | prefix);
    }
}

void ByteWriter::WriteByte(char byte, int block) {
    file_.seekp(block_pos_[block] >> 3, std::ios_base::beg);
    if (block_unwritten_suffix_[block] != 0) {
        char suffix = block_unwritten_suffix_[block];
        int len = 9 - bit_str_len_[suffix];
        file_.put((suffix << len) | (byte >> (8 - len)));
        block_unwritten_suffix_[block] = (byte & ((1 << len) - 1));
        block_pos_[block] += 8;
    }
    else {
        int len = block_pos_[block] & 7;
        char pad = (byte >> len);
        block_unwritten_prefix_[block] = ((block_unwritten_prefix_[block] << (8 - len)) | pad);
        block_unwritten_suffix_[block] = ((1 << len) | (byte & ((1 << len) - 1)));
    }
}

void ByteWriter::WriteLess(char byte, int len, int block) {
    file_.seekp(block_pos_[block] >> 3, std::ios_base::beg);
    byte = (byte & ((1 << len) - 1));
    if (block_unwritten_suffix_[block] != 0) {
        char suffix = block_unwritten_suffix_[block];
        int needed_len = 9 - bit_str_len_[suffix];
        if (needed_len <= len) {
            len -= needed_len;
            file_.put((suffix << needed_len) | (byte >> len));
            byte = byte & ((1 << len) - 1);
            block_unwritten_suffix_[block] = 1;
            block_pos_[block] += needed_len;
        }
        block_unwritten_suffix_[block] = ((block_unwritten_suffix_[block] << len) | byte);
        block_pos_[block] += len;
    }
    else {
        int needed_len = 8 - (block_pos_[block] & 7);
        if (needed_len <= len) {
            char pad = (byte >> (len - needed_len));
            block_unwritten_prefix_[block] = ((block_unwritten_prefix_[block] << needed_len) | pad);
            char t = (1 << (len - needed_len));
            block_unwritten_suffix_[block] = (t | (byte & (t - 1)));
        }
        else {
            block_unwritten_prefix_[block] = ((block_unwritten_prefix_[block] << len) | byte);
        }
    }
}

}  // namespace db_compress
