#include "numerical_model.h"

#include "base.h"
#include "model.h"
#include "utility.h"

#include <vector>
#include <cmath>
#include <algorithm>

namespace db_compress {

namespace {

std::vector<size_t> GetPredictorCap(const Schema& schema, const std::vector<size_t>& pred) {
    std::vector<size_t> cap;
    for (size_t i = 0; i < pred.size(); ++i)
        cap.push_back(GetAttrInterpreter(schema.attr_type[pred[i]])->EnumCap());
    return cap;
}

}  // anonymous namespace

LaplaceProbTree::LaplaceProbTree(const LaplaceStats& stats, double err, bool target_int) :
    mean_(stats.median),
    dev_(stats.mean_abs_dev),
    target_int_(target_int),
    l_(0),
    r_(0),
    l_inf_(true),
    r_inf_(true),
    bin_size_(err * 2 + (target_int ? 1 : 0)) {
}

bool LaplaceProbTree::HasNextBranch() const {
    if (dev_ == 0) return false;
    return !(r_ == l_ && !l_inf_ && !r_inf_);
}

void LaplaceProbTree::GenerateNextBranch() {
    if (l_inf_ && r_inf_) {
        // Initial Branch
        prob_segs_ = std::vector<Prob>(2);
        double p = GetCDFExponential(dev_, bin_size_ / 2) / 2;
        Prob prob = GetProb(p);
        if (prob == GetZeroProb())
            prob = GetProb(1, 16);
        prob_segs_[0] = GetProb(1, 1) - prob;
        prob_segs_[1] = GetProb(1, 1) + prob;
    } else {
        Prob prob;
        prob_segs_ = std::vector<Prob>(1);
        if (l_inf_ || r_inf_) {
            int mid = ceil(dev_ / bin_size_);
            double p = GetCDFExponential(dev_, mid * bin_size_);
            if (l_inf_) {
                // Reversed
                prob = GetOneProb() - GetProb(p);
                mid_ = r_ - mid;
            } else {
                prob = GetProb(p);
                mid_ = l_ + mid - 1;
            }
        } else {
            int mid = (r_ - l_ + 1) / 2;
            double p = GetCDFExponential(dev_, mid * bin_size_) /
                       GetCDFExponential(dev_, (r_ - l_ + 1) * bin_size_);
            if (r_ < 0) {
                // Reversed
                prob = GetOneProb() - GetProb(p);
                mid_ = r_ - mid;
            } else {
                prob = GetProb(p);
                mid_ = l_ + mid - 1;
            }
        }
        prob_segs_[0] = prob;
    }
}

int LaplaceProbTree::GetNextBranch(const AttrValue* attr) const {
    double value;
    if (target_int_)
        value = static_cast<const IntegerAttrValue*>(attr)->Value();
    else
        value = static_cast<const DoubleAttrValue*>(attr)->Value();
    int branch;
    if (l_inf_ && r_inf_) {
        // Initial Branch
        if (fabs(value - mean_) <= bin_size_ / 2)
            branch = 1;
        else
            branch = (value >= mean_ ? 2 : 0);
    }
    else
        branch = (value - mean_ >= (mid_ + 0.5) * bin_size_ ? 1 : 0);
    return branch;
}

void LaplaceProbTree::ChooseNextBranch(int branch) {
    if (l_inf_ && r_inf_) {
        // Initial Branch
        if (branch == 0) {
            SetRight(-1);
        } else if (branch == 1) {
            SetLeft(0);
            SetRight(0);
        } else {
            SetLeft(1);
        }
    } else {
        if (branch == 0) {
            SetRight(mid_);
        } else {
            SetLeft(mid_ + 1);
        }
    }
}

AttrValue* LaplaceProbTree::GetResultAttr() const {
    if (!HasNextBranch()) {
        if (target_int_) {
            return new IntegerAttrValue((int)round(mean_ + l_ * bin_size_));
        } else {
            return new DoubleAttrValue(mean_ + l_ * bin_size_);
        }
    } else return NULL;
}

void LaplaceStats::PushValue(double value) {
    if (count == 0) {
        values.push_back(value);
        if (values.size() > 20)
            GetMedian(); 
    } else {
        ++ count;
        sum_abs_dev += fabs(value - median);
    }
}

void LaplaceStats::End() {
    if (values.size() > 0)
        GetMedian();
    mean_abs_dev = sum_abs_dev / count;

    QuantizationToFloat32Bit(&mean_abs_dev);
    QuantizationToFloat32Bit(&median);
}

void LaplaceStats::GetMedian() {
    sort(values.begin(), values.end());
    median = values[values.size() / 2];
    count = values.size();
    for (size_t i = 0; i < values.size(); ++i)
        sum_abs_dev += fabs(values[i] - median);
    values.clear();
}

TableLaplace::TableLaplace(const Schema& schema, 
                           const std::vector<size_t>& predictor_list, 
                           size_t target_var,
                           double err,
                           bool target_int) : 
    Model(predictor_list, target_var), 
    predictor_interpreter_(predictor_list_.size()),
    err_(err),
    target_int_(target_int),
    model_cost_(0),
    dynamic_list_(GetPredictorCap(schema, predictor_list)) {
    if (target_int_)
        err_ = floor(err_);
    QuantizationToFloat32Bit(&err_);
    for (size_t i = 0; i < predictor_list_.size(); ++i)
        predictor_interpreter_[i] = GetAttrInterpreter(schema.attr_type[predictor_list_[i]]);
}

ProbTree* TableLaplace::GetProbTree(const Tuple& tuple) {
    std::vector<size_t> index;
    GetDynamicListIndex(tuple, &index);
    prob_tree_.reset(new LaplaceProbTree(dynamic_list_[index], err_, target_int_));
    return prob_tree_.get();
}

void TableLaplace::GetDynamicListIndex(const Tuple& tuple, std::vector<size_t>* index) {
    index->clear();
    for (size_t i = 0; i < predictor_list_.size(); ++i ) {
        const AttrValue* attr = tuple.attr[predictor_list_[i]];
        size_t val = predictor_interpreter_[i]->EnumInterpret(attr);
        index->push_back(val);
    }
}

void TableLaplace::FeedTuple(const Tuple& tuple) {
    std::vector<size_t> predictors;
    GetDynamicListIndex(tuple, &predictors);
    double target_val;
    const AttrValue* attr = tuple.attr[target_var_];
    if (target_int_)
        target_val = static_cast<const IntegerAttrValue*>(attr)->Value();
    else
        target_val = static_cast<const DoubleAttrValue*>(attr)->Value();
    LaplaceStats& stat = dynamic_list_[predictors];
    stat.PushValue(target_val);
}

void TableLaplace::EndOfData() {
    for (size_t i = 0; i < dynamic_list_.size(); ++i ) {
        LaplaceStats& stat = dynamic_list_[i];
        stat.End();

        if (stat.mean_abs_dev != 0) {
            if (target_int_)
                model_cost_ += stat.count * (log(stat.mean_abs_dev) 
                                             - log(err_ + 0.5) + 1) / log(2);
            else
                model_cost_ += stat.count * (log(stat.mean_abs_dev) 
                                             - log(err_) + 1) / log(2);
        }
    }
    model_cost_ += GetModelDescriptionLength();
}

int TableLaplace::GetModelDescriptionLength() const {
    size_t table_size = dynamic_list_.size();;
    // See WriteModel function for details of model description.
    return table_size * 64 + predictor_list_.size() * 16 + 48;
}

void TableLaplace::WriteModel(ByteWriter* byte_writer,
                               size_t block_index) const {
    unsigned char bytes[4];
    byte_writer->WriteByte(predictor_list_.size(), block_index);
    ConvertSinglePrecision(err_, bytes);
    byte_writer->Write32Bit(bytes, block_index);

    for (size_t i = 0; i < predictor_list_.size(); ++i )
        byte_writer->Write16Bit(predictor_list_[i], block_index);

    // Write Model Parameters
    size_t table_size = dynamic_list_.size();
    for (size_t i = 0; i < table_size; ++i ) {
        const LaplaceStats& stat = dynamic_list_[i];
        ConvertSinglePrecision(stat.median, bytes);
        byte_writer->Write32Bit(bytes, block_index);
        ConvertSinglePrecision(stat.mean_abs_dev, bytes);
        byte_writer->Write32Bit(bytes, block_index);
    }
}

Model* TableLaplace::ReadModel(ByteReader* byte_reader, 
                               const Schema& schema, size_t target_var, bool target_int) {
    size_t predictor_size = byte_reader->ReadByte();
    unsigned char bytes[4];
    byte_reader->Read32Bit(bytes);
    double err = ConvertSinglePrecision(bytes);

    std::vector<size_t> predictor_list;
    for (size_t i = 0; i < predictor_size; ++i )
        predictor_list.push_back(byte_reader->Read16Bit());
    TableLaplace* model = new TableLaplace(schema, predictor_list, target_var, err, target_int); 

    // Write Model Parameters
    size_t table_size = model->dynamic_list_.size();
    for (size_t i = 0; i < table_size; ++i ) {
        LaplaceStats& stat = model->dynamic_list_[i];
        byte_reader->Read32Bit(bytes);
        stat.median = ConvertSinglePrecision(bytes);
        byte_reader->Read32Bit(bytes);
        stat.mean_abs_dev = ConvertSinglePrecision(bytes);
    }
    
    return model;    
}

Model* TableLaplaceRealCreator::ReadModel(ByteReader* byte_reader, 
                                          const Schema& schema, size_t index) {
    return TableLaplace::ReadModel(byte_reader, schema, index, false);
}

Model* TableLaplaceRealCreator::CreateModel(const Schema& schema,
            const std::vector<size_t>& predictor, size_t index, double err) {
    for (size_t i = 0; i < predictor.size(); ++i) {
        int attr_type = schema.attr_type[predictor[i]];
        if (!GetAttrInterpreter(attr_type)->EnumInterpretable())
            return NULL;
    }
    return new TableLaplace(schema, predictor, index, err, false);
}

Model* TableLaplaceIntCreator::ReadModel(ByteReader* byte_reader, 
                                         const Schema& schema, size_t index) {
    return TableLaplace::ReadModel(byte_reader, schema, index, true);
}

Model* TableLaplaceIntCreator::CreateModel(const Schema& schema,
            const std::vector<size_t>& predictor, size_t index, double err) {
    for (size_t i = 0; i < predictor.size(); ++i) {
        int attr_type = schema.attr_type[predictor[i]];
        if (!GetAttrInterpreter(attr_type)->EnumInterpretable())
            return NULL;
    }
    return new TableLaplace(schema, predictor, index, err, true);
}

}  // namespace db_compress
