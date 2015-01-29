#include "gaussian_model.h"

#include "attribute.h"
#include "base.h"
#include "model.h"
#include "utility.h"

#include <vector>
#include <cmath>

namespace db_compress {

std::vector<size_t> TableGaussian::GetPredictorList(const Schema& schema,
                                          const std::vector<size_t>& predictor_list) {
    std::vector<size_t> ret;
    for (size_t i = 0; i < predictor_list.size(); ++i )
    if ( GetBaseType(schema.attr_type[predictor_list[i]]) == BASE_TYPE_ENUM )
        ret.push_back(predictor_list[i]);
    return ret;
}

TableGaussian::TableGaussian(const Schema& schema, 
                             const std::vector<size_t>& predictor_list, 
                             size_t target_var,
                             bool predict_int, 
                             double err) : 
    predictor_list_(GetPredictorList(schema, predictor_list)), 
    predictor_range_(predictor_list_.size()),
    target_var_(target_var),
    err_(err),
    target_int_(predict_int),
    model_cost_(0),
    dynamic_list_(predictor_list_.size()) {}

ProbDist* TableGaussian::GetProbDist(const Tuple& tuple, 
                                     const ProbInterval& prob_interval) {
    //Todo:
}

void TableGaussian::GetDynamicListIndex(const Tuple& tuple, std::vector<size_t>* index) {
    index->clear();
    for (size_t i = 0; i < predictor_list_.size(); ++i ) {
        AttrValue* attr = tuple.attr[predictor_list_[i]];
        size_t val = static_cast<EnumAttrValue*>(attr)->Value();
        if (val >= predictor_range_[i]) 
            predictor_range_[i] = val + 1;
        index->push_back(val);
    }
}

ProbInterval TableGaussian::GetProbInterval(const Tuple& tuple, 
                                            const ProbInterval& prob_interval, 
                                            std::vector<unsigned char>* emit_bytes) {
    //Todo:
}

const std::vector<size_t>& TableGaussian::GetPredictorList() const {
    return predictor_list_;
}

size_t TableGaussian::GetTargetVar() const {
    return target_var_;
}

int TableGaussian::GetModelCost() const {
    return model_cost_;
}

void TableGaussian::FeedTuple(const Tuple& tuple) {
    std::vector<size_t> predictors;
    GetDynamicListIndex(tuple, &predictors);
    double target_val;
    if (target_int_)
        target_val = static_cast<IntegerAttrValue*>(tuple.attr[target_var_])->Value();
    else
        target_val = static_cast<DoubleAttrValue*>(tuple.attr[target_var_])->Value();
    GaussianStats& stat = dynamic_list_[predictors];
    ++ stat.count;
    stat.sum += target_val;
    stat.sqr_sum += target_val * target_val;
}

void TableGaussian::EndOfData() {
    for (size_t i = 0; i < dynamic_list_.size(); ++i ) {
        GaussianStats& vec = dynamic_list_[i];
        vec.mean = vec.sum / vec.count;
        vec.std = sqrt(vec.sqr_sum / vec.count - vec.mean * vec.mean);
        // Todo: Quantization and Model Cost Update
    }
}

int TableGaussian::GetModelDescriptionLength() const {
    // Todo:
}

void TableGaussian::WriteModel(ByteWriter* byte_writer,
                               size_t block_index) const {
    // Todo:
}

}  // namespace db_compress
