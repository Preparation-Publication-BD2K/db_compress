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
    err_(err),
    description_length_(0)  {
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
    std::vector<int> predictors;
    for (int i = 0; i < predictor_list_.size(); ++i) {
        int val = static_cast<EnumAttrValue*>(tuple.attr[predictor_list_[i]])->Value();
        predictors.push_back(val);
    }
    int target_val = static_cast<EnumAttrValue*>(tuple.attr[target_var_])->Value();
    const std::vector<double>& vec = dynamic_list_.GetValue(predictors);
    double l = (target_val == 0 ? 0 : vec[target_val - 1]);
    double r = (target_val == vec.size() - 1 ? 1 : vec[target_val]);
    double span = prob_interval.r - prob_interval.l;
    ProbInterval ret(span * l + prob_interval.l, span * r + prob_interval.l);
    return ret;
}

const std::vector<int>& TableCategorical::GetPredictorList() const {
    return predictor_list_;
}

int TableCategorical::GetTargetVar() const {
    return target_var_;
}

int TableCategorical::GetModelCost() const {
    return description_length_;
}

void TableCategorical::FeedTuple(const Tuple& tuple) {
    std::vector<int> predictor_value;
    for (int i = 0; i < predictor_list_.size(); ++i ) {
        int val = static_cast<EnumAttrValue*>(tuple.attr[predictor_list_[i]])->Value();
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
        std::vector<double>& count = dynamic_list_.GetValue(i);
        // We might need to resize count vector
        count.resize(target_range_);
        std::vector<double> prob;

        // Mark empty entries
        std::vector<bool> is_zero;
        for (int j = 0; j < count.size(); j++ )
            if (count[j] == 0)
                is_zero.push_back(true);
            else
                is_zero.push_back(false);
    
        // Calculate Probability Vector
        double total_count = count[0];
        for (int j = 1; j < count.size(); j++ ) {
            prob.push_back(total_count);
            total_count += count[j];
        }
        for (int j = 0; j < prob.size(); j++ )
            prob[j] /= total_count;
        // Quantization
        for (int j = 0; j < prob.size(); j++ )
            prob[j] = round(prob[j] * 256) / 256;
        // All the following, we try to avoid zero probability
        if (!is_zero[0] && prob[0] == 0) {
            prob[0] = (double)1.0 / 256;
        }
        for (int j = 1; j < prob.size(); j++ )
        if (!is_zero[j] && prob[j] <= prob[j - 1]) {
            prob[j] = prob[j - 1] + (double)1.0 / 256;
        }
        if (!is_zero[count.size() - 1] && prob[count.size() - 1] == 1) {
            prob[count.size() - 1] = 1 - (double)1.0 / 256;
        }
        for (int j = prob.size() - 1; j >= 1; j-- )
        if (!is_zero[j] && prob[j] <= prob[j - 1]) {
            prob[j - 1] = prob[j] - (double)1.0 / 256;
        }
        // Update description length
        for (int j = 0; j < count.size(); j++ )
        if (!is_zero[j])
            description_length_ += count[j] * 
                (- log((j == count.size() - 1 ? 1 : prob[j])
                        - (j == 0 ? 0 : prob[j - 1])) ); 
        // Write back to dynamic list
        count = prob;
    }
    // Add table description length to model description length
    int table_size = 1;
    for (int i = 0; i < predictor_range_.size(); i++ )
        table_size *= predictor_range_[i];
    description_length_ += table_size * (target_range_ - 1) * 8;
}

TableGuassian::TableGuassian(const Schema& schema, 
                             const std::vector<int>& predictor_list, 
                             int target_var,
                             bool predict_int, 
                             double err) : 
    target_var_(target_var),
    err_(err),
    target_int_(predict_int),
    description_length_(0) {
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

int TableGuassian::GetModelCost() const {
    //Todo:
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

int StringModel::GetModelCost() const {
    //Todo:
}

}  // namespace db_compress
