#include "model.h"
#include "model_impl.h"
#include "attribute.h"

#include <vector>
#include <cmath>

namespace db_compress {

// ------------------------- TableCategorical ------------------------------
TableCategorical::TableCategorical(const Schema& schema, 
                                   const std::vector<size_t>& predictor_list, 
                                   size_t target_var, 
                                   double err) : 
    target_var_(target_var),
    target_range_(0),
    err_(err),
    model_cost_(0)  {
    predictor_list_.clear();
    for (size_t i = 0; i < predictor_list.size(); ++i )
    if ( GetBaseType(schema.attr_type[predictor_list[i]]) == BASE_TYPE_ENUM )
        predictor_list_.push_back(predictor_list[i]);
    predictor_range_ = std::vector<size_t>(predictor_list_.size());
}

ProbDist* TableCategorical::GetProbDist(const Tuple& tuple, 
                                        const ProbInterval& prob_interval) {
    //Todo:
}

ProbInterval TableCategorical::GetProbInterval(const Tuple& tuple, 
                                               const ProbInterval& prob_interval, 
                                               std::vector<char>* emit_bytes) {
    std::vector<size_t> predictors;
    for (size_t i = 0; i < predictor_list_.size(); ++i) {
        AttrValue* attr = tuple.attr[predictor_list_[i]];
        size_t val = static_cast<EnumAttrValue*>(attr)->Value();
        predictors.push_back(val);
    }
    size_t target_val = static_cast<EnumAttrValue*>(tuple.attr[target_var_])->Value();
    const std::vector<double>& vec = dynamic_list_[predictors];
    double l = (target_val == 0 ? 0 : vec[target_val - 1]);
    double r = (target_val == vec.size() - 1 ? 1 : vec[target_val]);
    double span = prob_interval.r - prob_interval.l;
    ProbInterval ret(span * l + prob_interval.l, span * r + prob_interval.l);
    return ret;
}

const std::vector<size_t>& TableCategorical::GetPredictorList() const {
    return predictor_list_;
}

size_t TableCategorical::GetTargetVar() const {
    return target_var_;
}

int TableCategorical::GetModelCost() const {
    return model_cost_;
}

void TableCategorical::FeedTuple(const Tuple& tuple) {
    std::vector<size_t> predictor_value;
    for (size_t i = 0; i < predictor_list_.size(); ++i ) {
        AttrValue* attr = tuple.attr[predictor_list_[i]];
        size_t val = static_cast<EnumAttrValue*>(attr)->Value();
        if (val >= predictor_range_[i]) 
            predictor_range_[i] = val + 1;
        predictor_value.push_back(val);
    }
    AttrValue* target_attr = tuple.attr[target_var_];
    size_t target_val = static_cast<EnumAttrValue*>(target_attr)->Value();
    if (target_val >= target_range_)
        target_range_ = target_val + 1;
    std::vector<double>& vec = dynamic_list_[predictor_value];
    if (vec.size() <= target_val) {
        vec.resize(target_val + 1);
        vec.shrink_to_fit();
    }
    ++ vec.at(target_val);
}

void TableCategorical::EndOfData() {
    for (size_t i = 0; i < dynamic_list_.size(); ++i ) {
        std::vector<double>& count = dynamic_list_[i];
        // We might need to resize count vector
        count.resize(target_range_);
        std::vector<double> prob;

        // Mark empty entries, since we are allowed to make mistakes,
        // we can mark entries that rarely appears as empty
        std::vector<bool> is_zero;
        double total_count = 0;
        for (size_t j = 0; j < count.size(); j++ )
            total_count += count[j];
        for (size_t j = 0; j < count.size(); j++ )
            if (count[j] <= total_count * err_)
                is_zero.push_back(true);
            else
                is_zero.push_back(false);
    
        // Calculate Probability Vector
        total_count = (is_zero[0] ? 0 : count[0]);
        for (size_t j = 1; j < count.size(); j++ ) {
            prob.push_back(total_count);
            if (!is_zero[j])
                total_count += count[j];
        }
        for (size_t j = 0; j < prob.size(); j++ )
            prob[j] /= total_count;

        // Quantization
        for (size_t j = 0; j < prob.size(); j++ )
            prob[j] = round(prob[j] * 255) / 255;
        // All the following, we try to avoid zero probability
        if (!is_zero[0] && prob[0] == 0) {
            prob[0] = (double)1.0 / 255;
        }
        for (size_t j = 1; j < prob.size(); j++ )
        if (!is_zero[j] && prob[j] <= prob[j - 1]) {
            prob[j] = prob[j - 1] + (double)1.0 / 255;
        }
        if (!is_zero[count.size() - 1] && prob[count.size() - 1] == 1) {
            prob[count.size() - 1] = 1 - (double)1.0 / 255;
        }
        for (size_t j = prob.size() - 1; j >= 1; j-- )
        if (!is_zero[j] && prob[j] <= prob[j - 1]) {
            prob[j - 1] = prob[j] - (double)1.0 / 255;
        }
        // Update model cost
        for (size_t j = 0; j < count.size(); j++ )
        if (!is_zero[j]) {
            model_cost_ += count[j] * 
                (- log2((j == count.size() - 1 ? 1 : prob[j])
                        - (j == 0 ? 0 : prob[j - 1])) ); 
        }
        // Write back to dynamic list
        count = prob;
    }
    // Add model description length to model cost
    model_cost_ += GetModelDescriptionLength();
}

int TableCategorical::GetModelDescriptionLength() const {
    size_t table_size = 1;
    for (size_t i = 0; i < predictor_range_.size(); i++ )
        table_size *= predictor_range_[i];
    // See WriteModel function for details of model description.
    return table_size * (target_range_ - 1) * 8 
            + predictor_list_.size() * 8 
            + predictor_range_.size() * 8 + 16;
}

void TableCategorical::WriteModel(ByteWriter* byte_writer,
                                  size_t block_index) const {
    // Write Model Description Prefix
    byte_writer->WriteByte(Model::TABLE_CATEGORY, block_index);
    byte_writer->WriteByte(predictor_list_.size(), block_index);
    for (size_t i = 0; i < predictor_list_.size(); i++ )
        byte_writer->WriteByte(predictor_list_[i], block_index);
    for (size_t i = 0; i < predictor_range_.size(); i++ )
        byte_writer->WriteByte(predictor_range_[i], block_index);

    // Write Model Parameters
    size_t table_size = 1;
    for (size_t i = 0; i < predictor_range_.size(); i++ )
        table_size *= predictor_range_[i];
    
    for (size_t i = 0; i < table_size; i++ ) {
        std::vector<size_t> predictors; 
        size_t t = i;
        for (size_t j = 0; j < predictor_range_.size(); j++ ) {
            predictors.push_back(t % predictor_range_[j]);
            t /= predictor_range_[j];
        }
        std::vector<double> prob_segs = dynamic_list_[predictors];
        for (size_t j = 0; j < prob_segs.size(); j++ ) {
            byte_writer->WriteByte((int)round(prob_segs[j] * 255), block_index);
        }
    }
}

// ------------------------- TableGuassian ------------------------------
TableGuassian::TableGuassian(const Schema& schema, 
                             const std::vector<size_t>& predictor_list, 
                             size_t target_var,
                             bool predict_int, 
                             double err) : 
    target_var_(target_var),
    err_(err),
    target_int_(predict_int),
    description_length_(0) {
    predictor_list_.clear();
    for (size_t i = 0; i < predictor_list.size(); ++i )
    if ( GetBaseType(schema.attr_type[predictor_list[i]]) == BASE_TYPE_ENUM )
        predictor_list_.push_back(predictor_list[i]);
    predictor_range_ = std::vector<size_t>(predictor_list_.size());
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

const std::vector<size_t>& TableGuassian::GetPredictorList() const {
    return predictor_list_;
}

size_t TableGuassian::GetTargetVar() const {
    return target_var_;
}

int TableGuassian::GetModelCost() const {
    //Todo:
}

void TableGuassian::FeedTuple(const Tuple& tuple) {
    std::vector<size_t> predictor_value;
    for (size_t i = 0; i < predictor_list_.size(); ++i ) {
        size_t val = static_cast<EnumAttrValue*>(tuple.attr[i])->Value();
        if (val >= predictor_range_[i]) 
            predictor_range_[i] = val + 1;
        predictor_value.push_back(val);
    }
    double target_val;
    if (target_int_)
        target_val = static_cast<IntegerAttrValue*>(tuple.attr[target_var_])->Value();
    else
        target_val = static_cast<DoubleAttrValue*>(tuple.attr[target_var_])->Value();
    GuassStats& stat = dynamic_list_[predictor_value];
    ++ stat.count;
    stat.sum += target_val;
    stat.sqr_sum += target_val * target_val;
}

void TableGuassian::EndOfData() {
    for (size_t i = 0; i < dynamic_list_.size(); ++i ) {
        GuassStats& vec = dynamic_list_[i];
        vec.mean = vec.sum / vec.count;
        vec.std = sqrt(vec.sqr_sum / vec.count - vec.mean * vec.mean);
    }
}

int TableGuassian::GetModelDescriptionLength() const {
    // Todo:
}

void TableGuassian::WriteModel(ByteWriter* byte_writer,
                               size_t block_index) const {
    // Todo:
}

// ------------------------- StringModel ------------------------------
StringModel::StringModel(size_t target_var) : target_var_(target_var) {}

ProbDist* StringModel::GetProbDist(const Tuple& tuple, const ProbInterval& prob_interval) {
    // Todo:
}

ProbInterval StringModel::GetProbInterval(const Tuple& tuple, 
                                          const ProbInterval& prob_interval, 
                                          std::vector<char>* emit_bytes) {
    // Todo:
}

const std::vector<size_t>& StringModel::GetPredictorList() const {
    return predictor_list_;
}

size_t StringModel::GetTargetVar() const { 
    return target_var_;
}

int StringModel::GetModelCost() const {
    //Todo:
}

int StringModel::GetModelDescriptionLength() const {
    return 8;
}

void StringModel::WriteModel(ByteWriter* byte_writer,
                             size_t block_index) const {
    // Todo: 
}

}  // namespace db_compress
