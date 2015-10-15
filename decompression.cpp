#include "base.h"
#include "decompression.h"

#include <fstream>
#include <vector>

namespace db_compress {

namespace {

Model* GetModelFromDescription(ByteReader* byte_reader, const Schema& schema, size_t index) {
    Model* ret;
    unsigned char creator_index = byte_reader->ReadByte();
    ret = GetAttrModel(schema.attr_type[index])[creator_index]
            ->ReadModel(byte_reader, schema, index);
    return ret;
}

}  // anonymous namespace

Decompressor::Decompressor(const char* compressedFileName, const Schema& schema) : 
    byte_reader_(compressedFileName),
    schema_(schema) {
}

void Decompressor::Init() {
    implicit_length_ = byte_reader_.ReadByte();
    for (size_t i = 0; i < schema_.attr_type.size(); ++ i) {
        attr_order_.push_back(byte_reader_.Read16Bit());
    }
    for (size_t i = 0; i < schema_.attr_type.size(); ++ i) {
        std::unique_ptr<Model> model(GetModelFromDescription(&byte_reader_, schema_, i));
        model_.push_back(std::move(model));
    }
    implicit_prefix_ = 0;
    ReadTuplePrefix();
}

void Decompressor::ReadTuplePrefix() {
    while (implicit_prefix_ < ((unsigned)1 << implicit_length_)) {
        bool bit = byte_reader_.ReadBit();
        if (bit) 
            ++ implicit_prefix_;
        else
            break;
    }
}

void Decompressor::ReadNextTuple(Tuple* tuple) {
    unsigned int implicit_prefix_count_ = 1;
    ProbInterval PIt(GetZeroProb(), GetOneProb());
    UnitProbInterval PIb = GetWholeProbInterval();
    for (size_t i = 0; i < schema_.attr_type.size(); ++i) {
        Decoder* decoder = model_[attr_order_[i]]->GetDecoder(*tuple, PIt, PIb);
        while (!decoder->IsEnd()) {
            bool bit;
            if (implicit_prefix_count_ <= implicit_length_) {
                bit = ((implicit_prefix_ >> (implicit_length_ - implicit_prefix_count_)) & 1); 
                ++ implicit_prefix_count_;
            } else
                bit = byte_reader_.ReadBit();
            decoder->FeedBit(bit);
        }
        PIt = decoder->GetPIt();
        PIb = decoder->GetPIb();
        const AttrValue* result = decoder->GetResult();
        tuple->attr[attr_order_[i]] = result;
    }
    // We read the prefix for next tuple after finish reading the current tuple,
    // this helps us to determine the end of file
    ReadTuplePrefix();
}

bool Decompressor::HasNext() const {
    return (implicit_prefix_ < ((unsigned)1 << implicit_length_)); 
}

}  // namespace db_compress
