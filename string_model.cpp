#include "string_model.h"

#include "attribute.h"
#include "base.h"
#include "model.h"
#include "utility.h"

#include <cmath>
#include <vector>
#include <string>

namespace db_compress {

StringModel::StringModel(size_t target_var) : 
    target_var_(target_var),
    char_prob_(256),
    length_prob_(64) {}

ProbDist* StringModel::GetProbDist(const Tuple& tuple, const ProbInterval& prob_interval) {
    // Todo:
}

void StringModel::GetProbInterval(const Tuple& tuple, 
                                  std::vector<ProbInterval>* prob_intervals, 
                                  std::unique_ptr<AttrValue>* result_attr) {
    // Since StringModel is lossless, we don't need to do anything if prob_interval is NULL
    if (prob_intervals == NULL) return;
    AttrValue* attr = tuple.attr[target_var_].get();
    std::string str = static_cast<StringAttrValue*>(attr)->Value();
    
    double l, r;
    if (str.length() < 63) {
        l = (str.length() == 0 ? 0 : length_prob_[str.length() - 1]);
        r = length_prob_[str.length()];
    } else {
        l = length_prob_[62];
        r = 1;
    }
    prob_intervals->push_back(ProbInterval(l, r));

    for (size_t i = 0; i < str.length(); i++ ) {
        unsigned char ch = str[i];
        l = char_prob_[ch - 1];
        r = (ch == 255 ? 1 : char_prob_[ch]);
        prob_intervals->push_back(ProbInterval(l, r));
    }
    if (str.length() >= 63) {
        l = 0;
        r = char_prob_[0];
        prob_intervals->push_back(ProbInterval(l, r));
    }
}

const std::vector<size_t>& StringModel::GetPredictorList() const {
    return predictor_list_;
}

size_t StringModel::GetTargetVar() const { 
    return target_var_;
}

void StringModel::FeedTuple(const Tuple& tuple) {
    AttrValue* attr = tuple.attr[target_var_].get();
    std::string str = static_cast<StringAttrValue*>(attr)->Value();
    for (size_t i = 0; i < str.length(); i++ )
        char_prob_[(unsigned char)str[i]] ++;
    if (str.length() >= 63) {
        length_prob_[63] ++;
        char_prob_[0] ++;
    } else {
        length_prob_[str.length()] ++;
    }
}

void StringModel::EndOfData() {
    // Calculate the probability vector of characters
    std::vector<double> prob;
    Quantization(&prob, char_prob_, 65535);
    char_prob_ = prob;
    // Calculate the probability vector of string lengths
    Quantization(&prob, length_prob_, 255);
    length_prob_ = prob;    
}

int StringModel::GetModelCost() const {
    // Since we only have one type of string model, this number does not matter.
    return 0;
}

int StringModel::GetModelDescriptionLength() const {
    return 8 + 255 * 16 + 63 * 8;
}

void StringModel::WriteModel(ByteWriter* byte_writer,
                             size_t block_index) const {
    byte_writer->WriteByte(Model::STRING_MODEL, block_index);
    for (int i = 0; i < 255; i++ ) {
        int code = round(char_prob_[i] * 65535);
        byte_writer->Write16Bit(code, block_index);
    } 
    for (int i = 0; i < 63; i++ )
        byte_writer->WriteByte((int)round(length_prob_[i] * 255), block_index);
}

Model* StringModel::ReadModel(ByteReader* byte_reader) {
    // Todo
}

}  // namespace db_compress
