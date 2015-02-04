#include "numerical_model.h"

#include "attribute.h"
#include "base.h"
#include "model.h"
#include "utility.h"

#include <vector>
#include <cmath>
#include <algorithm>

namespace db_compress {

std::vector<size_t> TableLaplace::GetPredictorList(const Schema& schema,
                                          const std::vector<size_t>& predictor_list) {
    std::vector<size_t> ret;
    for (size_t i = 0; i < predictor_list.size(); ++i )
    if ( GetBaseType(schema.attr_type[predictor_list[i]]) == BASE_TYPE_ENUM )
        ret.push_back(predictor_list[i]);
    return ret;
}

TableLaplace::TableLaplace(const Schema& schema, 
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
    dynamic_list_(predictor_list_.size()) {
    if (target_int_)
        err_ = floor(err_);
}

ProbDist* TableLaplace::GetProbDist(const Tuple& tuple, 
                                     const ProbInterval& prob_interval) {
    //Todo:
}

void TableLaplace::GetDynamicListIndex(const Tuple& tuple, std::vector<size_t>* index) {
    index->clear();
    for (size_t i = 0; i < predictor_list_.size(); ++i ) {
        AttrValue* attr = tuple.attr[predictor_list_[i]].get();
        size_t val = static_cast<EnumAttrValue*>(attr)->Value();
        if (val >= predictor_range_[i]) 
            predictor_range_[i] = val + 1;
        index->push_back(val);
    }
}

ProbInterval TableLaplace::GetProbInterval(const Tuple& tuple, 
                                            const ProbInterval& prob_interval, 
                                            std::vector<unsigned char>* emit_bytes,
                                            std::unique_ptr<AttrValue>* result_attr) {
    AttrValue* attr = tuple.attr[target_var_].get();
    double target_val;
    if (target_int_)
        target_val = static_cast<IntegerAttrValue*>(attr)->Value();
    else
        target_val = static_cast<DoubleAttrValue*>(attr)->Value();

    std::vector<size_t> predictors;
    GetDynamicListIndex(tuple, &predictors);
    const LaplaceStats& stat = dynamic_list_[predictors];
    double median = stat.median, lambda = stat.mean_abs_dev;

    double value_l = 0, value_r = 1e100;
    bool rhs;
    if (target_val > median) {
        target_val -= median;
        rhs = true;
    } else {
        target_val = median - target_val;
        rhs = false;
    }
    double l = 0, r = 1;
    while (value_r - value_l > 2 * err_) {
        double l_prob = 1 - exp(-value_l / lambda);
        double r_prob = 1 - exp(-value_r / lambda);
        double mid_prob = (l_prob + r_prob) / 2;
        double value_mid = - log(1 - mid_prob) * lambda;
        if (target_val < value_mid) {
            r = (l + r) / 2;
            value_r = value_mid;
        } else {
            l = (l + r) / 2;
            value_l = value_mid;
        }
    }
    target_val = (value_l + value_r) / 2;
    if (rhs) {
        l = 0.5 + l / 2;
        r = 0.5 + r / 2;
    } else {
        l = 0.5 - l / 2;
        r = 0.5 - r / 2;
        std::swap(l, r);
    }
    if (target_int_)
        result_attr->reset(new IntegerAttrValue((int)floor(target_val)));
    else
        result_attr->reset(new DoubleAttrValue(target_val));
    ProbInterval ret(0, 1);
    GetProbSubinterval(prob_interval.l, prob_interval.r, l, r, &ret.l, &ret.r, emit_bytes);
    return ret;
}

const std::vector<size_t>& TableLaplace::GetPredictorList() const {
    return predictor_list_;
}

size_t TableLaplace::GetTargetVar() const {
    return target_var_;
}

int TableLaplace::GetModelCost() const {
    return model_cost_;
}

void TableLaplace::FeedTuple(const Tuple& tuple) {
    std::vector<size_t> predictors;
    GetDynamicListIndex(tuple, &predictors);
    double target_val;
    AttrValue* attr = tuple.attr[target_var_].get();
    if (target_int_)
        target_val = static_cast<IntegerAttrValue*>(attr)->Value();
    else
        target_val = static_cast<DoubleAttrValue*>(attr)->Value();
    LaplaceStats& stat = dynamic_list_[predictors];
    if (stat.count == 0) {
        stat.values.push_back(target_val);
        if (stat.values.size() > 20) {
            sort(stat.values.begin(), stat.values.end());
            stat.median = stat.values[stat.values.size() / 2];
            stat.count = stat.values.size();
            for (size_t i = 0; i < stat.values.size(); ++i)
                stat.sum_abs_dev += fabs(stat.values[i] - stat.median);
            stat.values.clear();
        } 
    } else {
        ++ stat.count;
        stat.sum_abs_dev += fabs(target_val - stat.median);
    }
}

void TableLaplace::EndOfData() {
    for (size_t i = 0; i < dynamic_list_.size(); ++i ) {
        LaplaceStats& vec = dynamic_list_[i];
        vec.mean_abs_dev = vec.sum_abs_dev / vec.count;

        QuantizationToFloat32Bit(&vec.mean_abs_dev);
        QuantizationToFloat32Bit(&vec.median);
    }
}

int TableLaplace::GetModelDescriptionLength() const {
    size_t table_size = 1;
    for (size_t i = 0; i < predictor_range_.size(); ++i )
        table_size *= predictor_range_[i];
    // See WriteModel function for details of model description.
    return table_size * 64 + predictor_list_.size() * 8
            + predictor_range_.size() * 8 + 16;
}

void TableLaplace::WriteModel(ByteWriter* byte_writer,
                               size_t block_index) const {
    byte_writer->WriteByte(Model::TABLE_LAPLACE, block_index);
    byte_writer->WriteByte(predictor_list_.size(), block_index);
    for (size_t i = 0; i < predictor_list_.size(); ++i )
        byte_writer->WriteByte(predictor_list_[i], block_index);
    for (size_t i = 0; i < predictor_range_.size(); ++i )
        byte_writer->WriteByte(predictor_range_[i], block_index);

    // Write Model Parameters
    size_t table_size = 1;
    for (size_t i = 0; i < predictor_range_.size(); ++i )
        table_size *= predictor_range_[i];

    for (size_t i = 0; i < table_size; ++i ) {
        std::vector<size_t> predictors;
        size_t t = i;
        for (size_t j = 0; j < predictor_range_.size(); ++j ) {
            predictors.push_back(t % predictor_range_[j]);
            t /= predictor_range_[j];
        }
        const LaplaceStats& stat = dynamic_list_[predictors];
        unsigned char bytes[4];
        ConvertSinglePrecision(stat.median, bytes);
        for (int j = 0; j < 4; j++ )
            byte_writer->WriteByte(bytes[j], block_index);
        ConvertSinglePrecision(stat.mean_abs_dev, bytes);
        for (int j = 0; j < 4; j++ )
            byte_writer->WriteByte(bytes[j], block_index);
    }
}

}  // namespace db_compress
