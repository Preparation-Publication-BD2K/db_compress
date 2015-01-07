/*
 *
 *
 */

#include "compression.h"
#include "base.h"

#include <cstring>
#include <vector>

namespace db_compress {

namespace {

/*
 * BitStirng stucture stores bit strings in the format of:
 *   bits[0],   bits[1],    ..., bits[n]
 *   1-32 bits, 33-64 bits, ..., n*32+1-n*32+k bits
 *
 *   bits[n] = **************000000000000000
 *             |-- k bits---|---32-k bits--|
 *   length = n*32+k
 */
struct BitString {
    std::vector<int> bits;
    int length;
};

inline char GetByte(int bits, int start_pos) {
    return (char)(((bits << start_pos) >> 24) & 0xff);
}

void ConvertTupleToBitString(const Tuple& tuple, 
                             const std::vector< std::unique_ptr<Model> >& model, 
                             const std::vector<int>& attr_order, 
                             BitString* bit_string) {
    bit_string->bits.clear(); 
    bit_string->length = 0;
    ProbInterval prob_interval(0, 1);
    for (int attr : attr_order) {
        std::vector<char> emit_byte;
        prob_interval = model[attr]->GetProbInterval(tuple, prob_interval, &emit_byte);
        for (int i = 0; i < emit_byte.size(); ++i) {
            int index = bit_string->length / 32;
            bit_string->bits[index] <<= 8;
            bit_string->bits[index] |= emit_byte[i];
            bit_string->length += 8;
            if ((bit_string->length & 31) == 0)
                bit_string->bits.push_back(0);
        }
        while (prob_interval.l > 0 || prob_interval.r < 1) {
            int& last = bit_string->bits[bit_string->length / 32];
            if (0.5 - prob_interval.l > prob_interval.r - 0.5) {
                last = (last << 1);
                prob_interval.r *= 2;
                prob_interval.l *= 2;
            } else {
                last = (last << 1) + 1;
                prob_interval.l = prob_interval.l * 2 - 1;
                prob_interval.r = prob_interval.r * 2 - 1;
            }
            bit_string->length ++;
            if ((bit_string->length & 31) == 0)
                bit_string->bits.push_back(0);
        }
    }
}

// PadBitString is used to pad zeros in the bit string as suffixes 
void PadBitString(BitString* bit_string, int target_length) {
    bit_string->length = target_length;
}

/*
 * Note that prefix_length is always less than 32 (We assume that the index of
 * the blocks can fit in 32-bit int).
 */
int ComputePrefixIndex(const BitString& bit_string, int prefix_length) {
    int shift_bit = 32 - prefix_length;
    return (bit_string.bits[0] >> shift_bit) & ((1 << prefix_length) - 1);
}

/*
 * Write the bit_string to byte_writer, ignores (prefix_length) bits at beginning.
 */
void WriteBitString(ByteWriter* byte_writer, const BitString& bit_string,
                    int prefix_length, int block_index) {
    while (prefix_length < bit_string.length) {
        int arr_index = (prefix_length >> 5);
        int end_of_block = std::min(bit_string.length, (arr_index << 5) + 32);
        if (end_of_block >= prefix_length + 8) {
            char byte = GetByte(bit_string.bits[arr_index], prefix_length & 31);
            byte_writer->WriteByte(byte, block_index);
            prefix_length += 8;
        } else {
            char byte = GetByte(bit_string.bits[arr_index], prefix_length & 31);
            int write_len = end_of_block - prefix_length;
            byte_writer->WriteLess(byte >> (8 - write_len), write_len, block_index);
            prefix_length += write_len;
        } 
    }
}

} // anonymous namespace

Compressor::Compressor(char *outputFile, const Schema& schema, const CompressionConfig& config) :
    outputFile_(outputFile),
    schema_(schema),
    learner_(new ModelLearner(schema, config)),
    stage_(0),
    num_of_tuples_(0) {}

void Compressor::ReadTuple(const Tuple& tuple) {
    switch (stage_) {
      case 0:
        // Learning Stage
        learner_->FeedTuple(tuple);
        num_of_tuples_ ++;
        break;
      case 1:
        // Scheduling Stage
        {
            BitString bit_string;
            ConvertTupleToBitString(tuple, model_, attr_order_, &bit_string);
            // If the bit_string is shorter than implicit_prefix_length_, we simply pad
            // zeros to the string, because the arithmetic code is prefix_code, such 
            // padding will not affect decoding.
            if (bit_string.length < implicit_prefix_length_) 
                PadBitString(&bit_string, implicit_prefix_length_);
            int prefix_index = ComputePrefixIndex(bit_string, implicit_prefix_length_);
            block_length_[prefix_index] += bit_string.length - implicit_prefix_length_ + 1;
        }    
        break;
      case 2:
        // Compressing Stage
        {
            BitString bit_string;
            ConvertTupleToBitString(tuple, model_, attr_order_, &bit_string);
            if (bit_string.length < implicit_prefix_length_) 
                PadBitString(&bit_string, implicit_prefix_length_);
            int prefix_index = ComputePrefixIndex(bit_string, implicit_prefix_length_);
            // We need to write the prefix 0 to indicate that the following is bit string 
            // representation of a tuple.
            byte_writer_->WriteLess(0, 1, prefix_index);
            WriteBitString(byte_writer_.get(), bit_string, implicit_prefix_length_, prefix_index);
        }    
        break;
    }
}

bool Compressor::RequireMoreIteration() const {
    return (stage_ != 3);
}

void Compressor::EndOfData() {
    switch (stage_) {
      case 0:
        learner_->EndOfData();
        if (!learner_->RequireMoreIterations()) {
            stage_ = 1;
            model_.resize(schema_.attr_type.size());
            for (int i = 0; i < schema_.attr_type.size(); i++ ) {
                std::unique_ptr<Model> ptr(learner_->GetModel(i));
                model_[i] = std::move(ptr);
            }
            attr_order_ = learner_->GetOrderOfAttributes();
            learner_.release();
            // Calculate length of implicit prefix
            implicit_prefix_length_ = 0;
            while ( (1 << implicit_prefix_length_) < num_of_tuples_ ) 
                implicit_prefix_length_ ++;
            block_length_ = std::vector<int>(1 << implicit_prefix_length_, 0);
        }
        num_of_tuples_ = 0;
        break;
      case 1:
        stage_ = 2;
        for (int i = 1; i < (1 << implicit_prefix_length_); i++ )
            block_length_[i] ++;
        byte_writer_.reset(new ByteWriter(&block_length_, outputFile_));
        for (int i = 1; i < (1 << implicit_prefix_length_); i++ )
            byte_writer_->WriteLess(1, 1, i);
        break;
      case 2:
        stage_ = 3;
        byte_writer_ = NULL;
        break;
    }
}

};
