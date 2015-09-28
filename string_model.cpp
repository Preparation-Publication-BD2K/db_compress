#include "string_model.h"

#include "base.h"
#include "categorical_model.h"
#include "model.h"
#include "utility.h"

#include <cmath>
#include <vector>
#include <string>

namespace db_compress {

StringProbTree::StringProbTree(const std::vector<Prob>& char_prob, 
                               const std::vector<Prob>& len_prob) :
    char_prob_(char_prob), 
    len_prob_(len_prob),
    is_end_(false),
    len_(-1),
    result_("") {}

bool StringProbTree::HasNextBranch() const {
    return !is_end_;
}

void StringProbTree::GenerateNextBranch() {
    if (len_ == -1)
        prob_segs_ = len_prob_;
    else if (result_.length() == 0)
        prob_segs_ = char_prob_;
}

int StringProbTree::GetNextBranch(const AttrValue* attr) const {
    const std::string& str = static_cast<const StringAttrValue*>(attr)->Value();
    if (len_ == -1)
        return (str.length() >= 63 ? 63 : str.length());
    else
        return (unsigned char)str[result_.length()];
}

void StringProbTree::ChooseNextBranch(int branch) {
    if (len_ == -1) {
        len_ = (branch == 63 ? -2 : branch);
        if (len_ == 0)
            is_end_ = true;
    } else {
        result_.push_back((char)branch);
        if (branch == 0 || (int)result_.length() == len_)
            is_end_ = true;
    }
}

AttrValue* StringProbTree::GetResultAttr() const {
    return new StringAttrValue(result_);
}

StringModel::StringModel(size_t target_var) : 
    Model(std::vector<size_t>(), target_var),
    char_count_(256),
    length_count_(64) {}

ProbTree* StringModel::GetProbTree(const Tuple& tuple) {
    prob_tree_.reset(new StringProbTree(char_prob_, length_prob_));
    return prob_tree_.get();
}

void StringModel::FeedTuple(const Tuple& tuple) {
    const AttrValue* attr = tuple.attr[target_var_];
    const std::string& str = static_cast<const StringAttrValue*>(attr)->Value();
    for (size_t i = 0; i < str.length(); i++ )
        char_count_[(unsigned char)str[i]] ++;
    if (str.length() >= 63) {
        length_count_[63] ++;
        char_count_[0] ++;
    } else {
        length_count_[str.length()] ++;
    }
}

void StringModel::EndOfData() {
    // Calculate the probability vector of characters
    Quantization(&char_prob_, char_count_, 16);
    char_count_.clear();
    // Calculate the probability vector of string lengths
    Quantization(&length_prob_, length_count_, 8);
    length_count_.clear();
}

int StringModel::GetModelCost() const {
    // Since we only have one type of string model, this number does not matter.
    return 0;
}

int StringModel::GetModelDescriptionLength() const {
    return 255 * 16 + 63 * 8;
}

void StringModel::WriteModel(ByteWriter* byte_writer,
                             size_t block_index) const {
    for (int i = 0; i < 255; ++i ) {
        int code = CastInt(char_prob_[i], 16);
        byte_writer->Write16Bit(code, block_index);
    } 
    for (int i = 0; i < 63; ++i ) {
        int code = CastInt(length_prob_[i], 8);
        byte_writer->WriteByte(code, block_index);
    }
}

Model* StringModel::ReadModel(ByteReader* byte_reader, size_t index) {
    StringModel* model = new StringModel(index);
    model->char_count_.clear();
    model->length_count_.clear();
    model->char_prob_.resize(255);
    model->length_prob_.resize(63);
    for (int i = 0; i < 255; ++i ) 
        model->char_prob_[i] = GetProb(byte_reader->Read16Bit(), 16);
    for (int i = 0; i < 63; ++i )
        model->length_prob_[i] = GetProb(byte_reader->ReadByte(), 8);
    return model;
}

Model* StringModelCreator::ReadModel(ByteReader* byte_reader, 
                                     const Schema& schema, size_t index) {
    return StringModel::ReadModel(byte_reader, index);
}

Model* StringModelCreator::CreateModel(const Schema& schema, const std::vector<size_t>& predictor,
                   size_t index, double err) {
    if (predictor.size() > 0) return NULL;
    return new StringModel(index);
}

}  // namespace db_compress
