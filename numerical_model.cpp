#include "numerical_model.h"

#include "attribute.h"
#include "base.h"
#include "model.h"
#include "utility.h"

#include <vector>
#include <cmath>
#include <algorithm>

namespace db_compress {

void LaplaceStats::GetMedian() {
    if (values.size() > 0) {
        sort(values.begin(), values.end());
        median = values[values.size() / 2];
        count = values.size();
        for (size_t i = 0; i < values.size(); ++i)
            sum_abs_dev += fabs(values[i] - median);
        values.clear();
    } 
}

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
    QuantizationToFloat32Bit(&err_);
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

void TableLaplace::GetProbInterval(const Tuple& tuple, 
                                   std::vector<ProbInterval>* prob_intervals,
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
    
    // If the laplace distribution is trivial, we don't need to do anything.
    if (lambda == 0) return;

    double result_val;
    if (target_val > median) {
        if (prob_intervals != NULL)
            prob_intervals->push_back(ProbInterval(0.5, 1));
        GetProbIntervalFromExponential(lambda, target_val - median, err_, target_int_,
                                       false, &result_val, prob_intervals);
        result_val += median;
    } else {
        if (prob_intervals != NULL)
            prob_intervals->push_back(ProbInterval(0, 0.5));
        GetProbIntervalFromExponential(lambda, median - target_val, err_, target_int_,
                                       true, &result_val, prob_intervals);
        result_val = median - result_val;
    }
    if (target_int_)
        result_attr->reset(new IntegerAttrValue((int)round(result_val)));
    else
        result_attr->reset(new DoubleAttrValue(result_val));
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
        if (stat.values.size() > 20)
            stat.GetMedian(); 
    } else {
        ++ stat.count;
        stat.sum_abs_dev += fabs(target_val - stat.median);
    }
}

void TableLaplace::EndOfData() {
    for (size_t i = 0; i < dynamic_list_.size(); ++i ) {
        LaplaceStats& stat = dynamic_list_[i];
        if (stat.values.size() > 0)
            stat.GetMedian();
        stat.mean_abs_dev = stat.sum_abs_dev / stat.count;

        QuantizationToFloat32Bit(&stat.mean_abs_dev);
        QuantizationToFloat32Bit(&stat.median);

        if (stat.mean_abs_dev != 0) {
            if (target_int_)
                model_cost_ += stat.count * (log(stat.mean_abs_dev) - log(err_ + 0.5) ) / log(2);
            else
                model_cost_ += stat.count * (log(stat.mean_abs_dev) - log(err_)) / log(2);
        }
    }
    model_cost_ += GetModelDescriptionLength();
}

int TableLaplace::GetModelDescriptionLength() const {
    size_t table_size = 1;
    for (size_t i = 0; i < predictor_range_.size(); ++i )
        table_size *= predictor_range_[i];
    // See WriteModel function for details of model description.
    return table_size * 64 + predictor_list_.size() * 16
            + predictor_range_.size() * 16 + 48;
}

void TableLaplace::WriteModel(ByteWriter* byte_writer,
                               size_t block_index) const {
    unsigned char bytes[4];
    byte_writer->WriteByte(Model::TABLE_LAPLACE, block_index);
    byte_writer->WriteByte(predictor_list_.size(), block_index);
    ConvertSinglePrecision(err_, bytes);
    byte_writer->Write32Bit(bytes, block_index);

    for (size_t i = 0; i < predictor_list_.size(); ++i )
        byte_writer->Write16Bit(predictor_list_[i], block_index);
    for (size_t i = 0; i < predictor_range_.size(); ++i )
        byte_writer->Write16Bit(predictor_range_[i], block_index);

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
        ConvertSinglePrecision(stat.median, bytes);
        byte_writer->Write32Bit(bytes, block_index);
        ConvertSinglePrecision(stat.mean_abs_dev, bytes);
        byte_writer->Write32Bit(bytes, block_index);
    }
}

Model* TableLaplace::ReadModel(ByteReader* byte_reader, const Schema& schema, size_t index) {
    size_t predictor_size = byte_reader->ReadByte();
    unsigned char bytes[4];
    byte_reader->Read32Bit(bytes);
    double err = ConvertSinglePrecision(bytes);
    bool target_int = (GetBaseType(schema.attr_type[index]) == BASE_TYPE_INTEGER);

    std::vector<size_t> predictor_list;
    for (size_t i = 0; i < predictor_size; ++i )
        predictor_list.push_back(byte_reader->Read16Bit());
    TableLaplace* model = new TableLaplace(schema, predictor_list, index, target_int, err); 
    for (size_t i = 0; i < predictor_size; ++i )
        model->predictor_range_[i] = byte_reader->Read16Bit();

    // Write Model Parameters
    size_t table_size = 1;
    for (size_t i = 0; i < predictor_size; ++i )
        table_size *= model->predictor_range_[i];

    for (size_t i = 0; i < table_size; ++i ) {
        std::vector<size_t> predictors;
        size_t t = i;
        for (size_t j = 0; j < predictor_size; ++j ) {
            predictors.push_back(t % model->predictor_range_[j]);
            t /= model->predictor_range_[j];
        }
        LaplaceStats& stat = model->dynamic_list_[predictors];
        byte_reader->Read32Bit(bytes);
        stat.median = ConvertSinglePrecision(bytes);
        byte_reader->Read32Bit(bytes);
        stat.mean_abs_dev = ConvertSinglePrecision(bytes);
    }
    
    return model;    
}

}  // namespace db_compress
