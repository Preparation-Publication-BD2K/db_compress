/*
 * Todo: add comment for compression.cpp
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
    std::vector<unsigned> bits;
    size_t length;
};

inline unsigned char GetByte(unsigned bits, int start_pos) {
    return (unsigned char)(((bits << start_pos) >> 24) & 0xff);
}

void ConvertTupleToBitString(const Tuple& tuple, 
                             const std::vector< std::unique_ptr<Model> >& model, 
                             const std::vector<size_t>& attr_order, 
                             BitString* bit_string) {
    bit_string->bits.clear(); 
    bit_string->length = 0;
    ProbInterval prob_interval(0, 1);
    for (size_t attr : attr_order) {
        std::vector<unsigned char> emit_byte;
        prob_interval = model[attr]->GetProbInterval(tuple, prob_interval, &emit_byte);
        for (size_t i = 0; i < emit_byte.size(); ++i) {
            size_t index = bit_string->length / 32;
            int offset = bit_string->length & 31;
            if (offset == 0)
                bit_string->bits.push_back(0);
            bit_string->bits[index] |= (emit_byte[i] << (24 - offset));
            bit_string->length += 8;
        }
    }
    while (prob_interval.l > 0 || prob_interval.r < 1) {
        int offset = bit_string->length & 31;
        if (offset == 0)
             bit_string->bits.push_back(0);
        unsigned& last = bit_string->bits[bit_string->length / 32];

        if (0.5 - prob_interval.l > prob_interval.r - 0.5) {
            prob_interval.r *= 2;
            prob_interval.l *= 2;
        } else {
            last |= (1 << (31 - offset));
            prob_interval.l = prob_interval.l * 2 - 1;
            prob_interval.r = prob_interval.r * 2 - 1;
        }
        bit_string->length ++;
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
int ComputePrefix(const BitString& bit_string, int prefix_length) {
    int shift_bit = 32 - prefix_length;
    return (bit_string.bits[0] >> shift_bit) & ((1 << prefix_length) - 1);
}

/*
 * Write the bit_string to byte_writer, ignores (prefix_length) bits at beginning.
 */
void WriteBitString(ByteWriter* byte_writer, const BitString& bit_string,
                    size_t prefix_length, size_t block_index) {
    while (prefix_length < bit_string.length) {
        size_t arr_index = (prefix_length >> 5);
        size_t end_of_block = (arr_index << 5) + 32;
        if (end_of_block > bit_string.length)
            end_of_block = bit_string.length;
        if (end_of_block >= prefix_length + 8) {
            unsigned char byte = GetByte(bit_string.bits[arr_index], prefix_length & 31);
            byte_writer->WriteByte(byte, block_index);
            prefix_length += 8;
        } else {
            unsigned char byte = GetByte(bit_string.bits[arr_index], prefix_length & 31);
            int write_len = end_of_block - prefix_length;
            byte_writer->WriteLess(byte >> (8 - write_len), write_len, block_index);
            prefix_length += write_len;
        } 
    }
}

} // anonymous namespace

Compressor::Compressor(const char *outputFile, const Schema& schema, 
                       const CompressionConfig& config) :
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
            int block_index = ComputePrefix(bit_string, implicit_prefix_length_) + 1;
            block_length_[block_index] += bit_string.length - implicit_prefix_length_ + 1;
        }    
        break;
      case 2:
        // Compressing Stage
        {
            BitString bit_string;
            ConvertTupleToBitString(tuple, model_, attr_order_, &bit_string);
            if (bit_string.length < implicit_prefix_length_) 
                PadBitString(&bit_string, implicit_prefix_length_);
            int block_index = ComputePrefix(bit_string, implicit_prefix_length_) + 1;
            // We need to write the prefix 0 of each tuple bit string
            byte_writer_->WriteLess(0, 1, block_index);
            WriteBitString(byte_writer_.get(), bit_string, implicit_prefix_length_, block_index);
        }    
        break;
    }
}

bool Compressor::RequireMoreIterations() const {
    return (stage_ != 3);
}

bool Compressor::RequireFullPass() const {
    return (stage_ > 0 || learner_->RequireFullPass());
}

/*
 * The meaning of stages are as follows:
 *  0: Model Learning Phase (multiple rounds)
 *  1: Writer Scheduling
 *  2: Writing
 *  3: End of Compression
 */
void Compressor::EndOfData() {
    switch (stage_) {
      case 0:
        learner_->EndOfData();
        if (!learner_->RequireMoreIterations()) {
            stage_ = 1;
            model_.resize(schema_.attr_type.size());
            for (size_t i = 0; i < schema_.attr_type.size(); i++ ) {
                std::unique_ptr<Model> ptr(learner_->GetModel(i));
                model_[i] = std::move(ptr);
            }
            attr_order_ = learner_->GetOrderOfAttributes();
            learner_ = NULL;
            // Calculate length of implicit prefix
            implicit_prefix_length_ = 0;
            while ( (unsigned)(1 << implicit_prefix_length_) < num_of_tuples_ ) 
                implicit_prefix_length_ ++;
            // Since the model occupies one block, there are 2^n + 1 blocks in total.
            block_length_ = std::vector<size_t>((1 << implicit_prefix_length_) + 1, 0);
        } else {
            // Reset the number of tuples, compute it again in the new round.
            num_of_tuples_ = 0;
        }
        break;
      case 1:
        stage_ = 2;
        // Compute Model Length
        block_length_[0] = 8 * schema_.attr_type.size() + 8;
        for (size_t i = 0; i < schema_.attr_type.size(); i++ )
            block_length_[0] += model_[i]->GetModelDescriptionLength();

        // Initialize Compressed File
        for (size_t i = 1; i < block_length_.size(); i++ )
            block_length_[i] ++;
        byte_writer_.reset(new ByteWriter(&block_length_, outputFile_));
        // Write Models
        byte_writer_->WriteByte(implicit_prefix_length_, 0);
        for (size_t i = 0; i < attr_order_.size(); i++ )
            byte_writer_->WriteByte(attr_order_[i], 0);
        for (size_t i = 0; i < schema_.attr_type.size(); i++ )
            model_[i]->WriteModel(byte_writer_.get(), 0);
        break;
      case 2:
        stage_ = 3;
        // Mark the end of each block, we can't user block_length_ anymore because
        // byte_writer_ has taken the ownership of this object.
        size_t num_of_blocks = (1 << implicit_prefix_length_) + 1;
        for (size_t i = 1; i < num_of_blocks; i++ )
            byte_writer_->WriteLess(1, 1, i);
        byte_writer_ = NULL;
        break;
    }
}

};
