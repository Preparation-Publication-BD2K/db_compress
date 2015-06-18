#include "data_io.h"

#include "base.h"

#include <iostream>
#include <vector>
#include <string>

namespace db_compress {

TupleIStream::TupleIStream(Tuple* tuple, const Schema& schema) :
    tuple_(tuple),
    schema_(schema),
    index_(0) {
}

TupleOStream::TupleOStream(const Tuple& tuple, const Schema& schema) :
    tuple_(tuple),
    schema_(schema),
    index_(0) {
}

/*
 * Since the prefix/suffix has at most 7 bits, we can use 0xff as special symbol to indicate
 * that the prefix/suffix is not used yet.
 */
ByteWriter::ByteWriter(std::vector<size_t>* block_length, const std::string& file_name) :
    block_unwritten_prefix_(block_length->size()),
    block_unwritten_suffix_(block_length->size(), 0xff),
    file_(file_name) {
    block_pos_.swap(*block_length);
    size_t total_len = 0;
    for (size_t i = 0; i < block_pos_.size(); i++) {
        size_t current_block_len = block_pos_[i];
        block_pos_[i] = total_len;
        total_len += current_block_len;
    }
    for (size_t i = 0; i < block_pos_.size(); i++) {
        if ((block_pos_[i] & 7) == 0)
            block_unwritten_suffix_[i] = 0;
    }
}

// Write all the remaining, we can use block_pos_ to identify the prefix/suffix length
// of corresponding blocks
ByteWriter::~ByteWriter() {
    for (size_t i = 1; i < block_pos_.size(); i++ )
    if ((block_pos_[i - 1] & 7) != 0) {
        size_t suffix_length = (block_pos_[i - 1] & 7);
        if (block_pos_[i] < block_pos_[i - 1] + (8 - suffix_length)) {
            size_t len = block_pos_[i] - block_pos_[i - 1];
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
    // The last block needs special care
    if ((block_pos_[block_pos_.size() - 1] & 7) != 0) {
        size_t pos = block_pos_[block_pos_.size() - 1];
        int pad = 8 - (pos & 7);
        file_.seekp(pos >> 3, std::ios_base::beg);
        file_.put(block_unwritten_suffix_[block_pos_.size() - 1] << pad);
    }
}

void ByteWriter::Write32Bit(unsigned char bytes[4], size_t block) {
    WriteByte(bytes[0], block);
    WriteByte(bytes[1], block);
    WriteByte(bytes[2], block);
    WriteByte(bytes[3], block);
}

void ByteWriter::Write16Bit(unsigned int val, size_t block) {
    val &= 65535;
    WriteByte(val >> 8, block);
    WriteByte(val & 255, block);
}

void ByteWriter::WriteByte(unsigned char byte, size_t block) {
    WriteLess(byte, 8, block);
}

void ByteWriter::WriteLess(unsigned char byte, size_t len, size_t block) {
    file_.seekp(block_pos_[block] >> 3, std::ios_base::beg);
    byte = (byte & ((1 << len) - 1)); 
    if (block_unwritten_suffix_[block] != 0xff) {
        unsigned char suffix = block_unwritten_suffix_[block];
        size_t needed_len = 8 - (block_pos_[block] & 7);
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
        size_t needed_len = 8 - (block_pos_[block] & 7);
        if (needed_len <= len) {
            len -= needed_len;
            unsigned char pad = (byte >> len);
            block_unwritten_prefix_[block] <<= needed_len;
            block_unwritten_prefix_[block] |= pad;
            byte = byte & ((1 << len) - 1);
            block_unwritten_suffix_[block] = byte;
            block_pos_[block] += needed_len + len;
        } else {
            block_unwritten_prefix_[block] = ((block_unwritten_prefix_[block] << len) | byte);
            block_pos_[block] += len;
        }
    }
}

unsigned char ByteReader::ReadByte() {
    // Todo
}

bool ByteReader::ReadBit() {
    // Todo
}

unsigned int ByteReader::Read16Bit() {
    // Todo
}

void ByteReader::Read32Bit(unsigned char* bytes) {
    // Todo
}

}  // namespace db_compress
