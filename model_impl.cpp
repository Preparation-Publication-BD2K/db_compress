#include "model.h"
#include "model_impl.h"
#include "attribute.h"

#include <vector>
#include <cmath>

namespace db_compress {

TableCategorical::TableCategorical(const Schema& schema, 
                                   const std::vector<int>& predictor_list, 
                                   int target_var, 
                                   double err) : 
    target_var_(target_var),
    target_range_(0),
    err_(err) {
    predictor_list_.clear();
    for (int i = 0; i < predictor_list.size(); ++i )
    if ( GetBaseType(schema.attr_type[predictor_list[i]]) == BASE_TYPE_ENUM )
        predictor_list_.push_back(predictor_list[i]);
    predictor_range_ = std::vector<int>(predictor_list_.size());
}

ProbDist* TableCategorical::GetProbDist(const Tuple& tuple, 
                                        const ProbInterval& prob_interval) {
    //Todo:
}

ProbInterval TableCategorical::GetProbInterval(const Tuple& tuple, 
                                               const ProbInterval& prob_interval, 
                                               std::vector<char>* emit_bytes) {
    //Todo:
}

const std::vector<int>& TableCategorical::GetPredictorList() const {
    return predictor_list_;
}

int TableCategorical::GetTargetVar() const {
    return target_var_;
}

void TableCategorical::FeedTuple(const Tuple& tuple) {
    std::vector<int> predictor_value;
    for (int i = 0; i < predictor_list_.size(); ++i ) {
        int val = static_cast<EnumAttrValue*>(tuple.attr[i])->Value();
        if (val >= predictor_range_[i]) 
            predictor_range_[i] = val + 1;
        predictor_value.push_back(val);
    }
    int target_val = static_cast<EnumAttrValue*>(tuple.attr[target_var_])->Value();
    if (target_val >= target_range_)
        target_range_ = target_val + 1;
    ++ dynamic_list_.GetValue(predictor_value).at(target_val);
}

void TableCategorical::EndOfData() {
    for (int i = 0; i < dynamic_list_.GetNumOfElements(); ++i ) {
        std::vector<int>& vec = dynamic_list_.GetValue(i);
        int sum = 0;
        for (int j = 0; j < vec.size(); j++ )
            sum += vec[j];
        vec.insert(vec.begin(), sum);
    }
}

TableGuassian::TableGuassian(const Schema& schema, 
                             const std::vector<int>& predictor_list, 
                             int target_var,
                             bool predict_int, 
                             double err) : 
    target_var_(target_var),
    err_(err),
    target_int_(predict_int) {
    predictor_list_.clear();
    for (int i = 0; i < predictor_list.size(); ++i )
    if ( GetBaseType(schema.attr_type[predictor_list[i]]) == BASE_TYPE_ENUM )
        predictor_list_.push_back(predictor_list[i]);
    predictor_range_ = std::vector<int>(predictor_list_.size());
}

ProbDist* TableGuassian::GetProbDist(const Tuple& tuple, 
                                     const ProbInterval& prob_interval) {
    //Todo:
}

ProbInterval TableGuassian::GetProbInterval(const Tuple& tuple, 
                                            const ProbInterval& prob_interval, 
                                            std::vector<char>* emit_bytes) {
    //Todo:
}

const std::vector<int>& TableGuassian::GetPredictorList() const {
    return predictor_list_;
}

int TableGuassian::GetTargetVar() const {
    return target_var_;
}

void TableGuassian::FeedTuple(const Tuple& tuple) {
    std::vector<int> predictor_value;
    for (int i = 0; i < predictor_list_.size(); ++i ) {
        int val = static_cast<EnumAttrValue*>(tuple.attr[i])->Value();
        if (val >= predictor_range_[i]) 
            predictor_range_[i] = val + 1;
        predictor_value.push_back(val);
    }
    double target_val;
    if (target_int_)
        target_val = static_cast<IntegerAttrValue*>(tuple.attr[target_var_])->Value();
    else
        target_val = static_cast<DoubleAttrValue*>(tuple.attr[target_var_])->Value();
    GuassStats& stat = dynamic_list_.GetValue(predictor_value);
    ++ stat.count;
    stat.sum += target_val;
    stat.sqr_sum += target_val * target_val;
}

void TableGuassian::EndOfData() {
    for (int i = 0; i < dynamic_list_.GetNumOfElements(); ++i ) {
        GuassStats& vec = dynamic_list_.GetValue(i);
        vec.mean = vec.sum / vec.count;
        vec.std = sqrt(vec.sqr_sum / vec.count - vec.mean * vec.mean);
    }
}

StringModel::StringModel(int target_var) : target_var_(target_var) {}

ProbDist* StringModel::GetProbDist(const Tuple& tuple, const ProbInterval& prob_interval) {
    // Todo:
}

ProbInterval StringModel::GetProbInterval(const Tuple& tuple, 
                                          const ProbInterval& prob_interval, 
                                          std::vector<char>* emit_bytes) {
    // Todo:
}

const std::vector<int>& StringModel::GetPredictorList() const {
    return predictor_list_;
}

int StringModel::GetTargetVar() const { 
    return target_var_;
}

}  // namespace db_compress
