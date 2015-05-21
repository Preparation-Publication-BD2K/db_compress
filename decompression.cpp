#include "base.h"
#include "decompression.h"

#include <fstream>
#include <memory>
#include <vector>

namespace db_compress {

Decompressor::Decompressor(const char* compressedFileName, const Schema& schema) : 
    byte_reader_(compressedFileName),
    schema_(schema) {}

void Decompressor::Init() {
    implicit_length_ = byte_reader_.ReadByte();
    for (size_t i = 0; i < schema_.attr_type.size(); ++ i) {
        attr_order_.push_back(byte_reader_.Read16Bit());
    }
    for (size_t i = 0; i < schema_.attr_type.size(); ++ i) {
        std::unique_ptr<Model> model(GetModelFromDescription(&byte_reader_));
        model_.push_back(std::move(model));
    }
}

void Decompressor::ReadNextTuple(Tuple* tuple) {
}

bool Decompressor::HasNext() {
}

}  // namespace db_compress
