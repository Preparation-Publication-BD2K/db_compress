#include "string_model.h"

#include "attribute.h"
#include "base.h"
#include "model.h"

#include <vector>
#include <string>

namespace db_compress {

StringModel::StringModel(size_t target_var) : target_var_(target_var) {}

ProbDist* StringModel::GetProbDist(const Tuple& tuple, const ProbInterval& prob_interval) {
    // Todo:
}

ProbInterval StringModel::GetProbInterval(const Tuple& tuple, 
                                          const ProbInterval& prob_interval, 
                                          std::vector<unsigned char>* emit_bytes, 
                                          std::unique_ptr<AttrValue>* result_attr) {
    // Todo: current logic is incorrect
    AttrValue* attr = tuple.attr[target_var_].get();
    std::string str = static_cast<StringAttrValue*>(attr)->Value();
    emit_bytes->clear();
    for (size_t i = 0; i < str.length(); i++ )
        emit_bytes->push_back(str[i]);
    emit_bytes->push_back(0);
    return prob_interval;
}

const std::vector<size_t>& StringModel::GetPredictorList() const {
    return predictor_list_;
}

size_t StringModel::GetTargetVar() const { 
    return target_var_;
}

int StringModel::GetModelCost() const {
    return 8;
}

int StringModel::GetModelDescriptionLength() const {
    return 8;
}

void StringModel::WriteModel(ByteWriter* byte_writer,
                             size_t block_index) const {
    byte_writer->WriteByte(Model::STRING_MODEL, block_index); 
}

}  // namespace db_compress
