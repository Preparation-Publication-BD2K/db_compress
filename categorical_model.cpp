#include "categorical_model.h"

#include "attribute.h"
#include "base.h"
#include "model.h"
#include "utility.h"

#include <vector>
#include <cmath>

namespace db_compress {

CategoricalProbDist::CategoricalProbDist(const std::vector<double>& prob_segs, 
                                         const ProbInterval& PIt, const ProbInterval& PIb) :
    prob_segs_(prob_segs),
    l_(0),
    r_(prob_segs.size()),
    PIt_(PIt),
    PIb_(PIb) {
    mid_ = (l_ + r_) / 2;
    if (l_ != r_)
        boundary_ = PIt_.l + (PIt_.r - PIt_.l) * prob_segs[mid_];
    else
        boundary_ = 0;
}

void CategoricalProbDist::Advance() {
    while (r_ > l_) {
        if (boundary_ >= PIb_.r) 
            r_ = mid_;
        else if (boundary_ <= PIb_.l)
            l_ = mid_ + 1;
        else break;
        if (l_ == r_) {
            ProbInterval sub(0, 1);
            sub.l = (l_ == 0 ? 0 : prob_segs[l_ - 1]);
            sub.r = (l_ == prob_segs.size() ? 1 : prob_segs[l_]);
            PIt_ = ReducePIProduct(PIt_, sub, NULL);
            ReducePI(&PIt_, &PIb_); 
            break;
        }

        mid_ = (l_ + r_) / 2;
        boundary_ = PIt_.l + (PIt_.r - PIt_.l) * prob_segs_[mid_];
    }
}

bool CategoricalProbDist::IsEnd() const {
    return (l_ == r_);
}

void CategoricalProbDist::FeedBit(bool bit) {
    double mid = (PIb_.l + PIb_.r) / 2;
    if (bit) 
        PIb_.l = mid;
    else 
        PIb_.r = mid;
    Advance();
}

ProbInterval CategoricalProbDist::GetPIt() const { return PIt_; }

ProbInterval CategoricalProbDist::GetPIb() const { return PIb_; }

AttrValue* CategoricalProbDist::GetResult() const {
    if (l_ == r_) return new EnumAttrValue(l_);
    else return NULL;
}

std::vector<size_t> TableCategorical::GetPredictorList(const Schema& schema,
                                          const std::vector<size_t>& predictor_list) {
    std::vector<size_t> ret;
    for (size_t i = 0; i < predictor_list.size(); ++i )
    if ( GetBaseType(schema.attr_type[predictor_list[i]]) == BASE_TYPE_ENUM )
        ret.push_back(predictor_list[i]);
    return ret;
}

TableCategorical::TableCategorical(const Schema& schema, 
                                   const std::vector<size_t>& predictor_list, 
                                   size_t target_var, 
                                   double err) :
    predictor_list_(GetPredictorList(schema, predictor_list)),
    predictor_range_(predictor_list_.size()),
    target_var_(target_var),
    target_range_(0),
    cell_size_(0),
    err_(err),
    model_cost_(0),
    dynamic_list_(predictor_list_.size()) {}

ProbDist* TableCategorical::GetProbDist(const Tuple& tuple, const ProbInterval& PIt,
                                        const ProbInterval& PIb) {
    std::vector<size_t> index;
    GetDynamicListIndex(tuple, &index);
    prob_dist_.reset(new CategoricalProbDist(dynamic_list_[index], PIt, PIb));
    return prob_dist_.get(); 
}

void TableCategorical::GetDynamicListIndex(const Tuple& tuple, std::vector<size_t>* index) {
    index->clear();
    for (size_t i = 0; i < predictor_list_.size(); ++i ) {
        AttrValue* attr = tuple.attr[predictor_list_[i]].get();
        size_t val = static_cast<EnumAttrValue*>(attr)->Value();
        if (val >= predictor_range_[i]) 
            predictor_range_[i] = val + 1;
        index->push_back(val);
    }
}

void TableCategorical::GetProbInterval(const Tuple& tuple, 
                                       std::vector<ProbInterval>* prob_intervals,
                                       std::unique_ptr<AttrValue>* result_attr) {
    std::vector<size_t> predictors;
    GetDynamicListIndex(tuple, &predictors);
    AttrValue* attr = tuple.attr[target_var_].get();
    size_t target_val = static_cast<EnumAttrValue*>(attr)->Value();

    const std::vector<double>& vec = dynamic_list_[predictors];
    double l = (target_val == 0 ? 0 : vec[target_val - 1]);
    double r = (target_val == vec.size() ? 1 : vec[target_val]);
    size_t new_val = target_val;
    if (r - l < 1e-9) {
        // We found the "omitted" categorical values. Recover it with the most likely category
        for (size_t i = 0; i <= vec.size(); i++ ) {
            double new_l = (i == 0 ? 0 : vec[i - 1]);
            double new_r = (i == vec.size() ? 1 : vec[i]);
            if (new_r - new_l > r - l) {
                l = new_l;
                r = new_r;
                new_val = i;
            }
        }
    }
    
    if (new_val != target_val)
        result_attr->reset(new EnumAttrValue(new_val));
    if (prob_intervals != NULL)
        prob_intervals->push_back(ProbInterval(l, r));
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
    std::vector<size_t> predictors;
    GetDynamicListIndex(tuple, &predictors);
    AttrValue* attr = tuple.attr[target_var_].get();
    size_t target_val = static_cast<EnumAttrValue*>(attr)->Value();
    if (target_val >= target_range_)
        target_range_ = target_val + 1;
   
    std::vector<double>& vec = dynamic_list_[predictors];
    if (vec.size() <= target_val) {
        vec.resize(target_val + 1);
        vec.shrink_to_fit();
    }
    ++ vec.at(target_val);
}

void TableCategorical::EndOfData() {
    // Determine cell size
    double quant_const;
    if (target_range_ > 255) {
        cell_size_ = 16;
        quant_const = 65535;
    } else {
        cell_size_ = 8;
        quant_const = 255;
    }
    for (size_t i = 0; i < dynamic_list_.size(); ++i ) {
        std::vector<double>& count = dynamic_list_[i];
        // We might need to resize count vector
        count.resize(target_range_);
        std::vector<double> prob;

        // Mark empty entries, since we are allowed to make mistakes,
        // we can mark entries that rarely appears as empty
        double total_count = 0, current_tol = 0;
        for (size_t j = 0; j < count.size(); j++ )
            total_count += count[j];
        for (size_t j = 0; j < count.size(); j++ )
        if (count[j] + current_tol <= total_count * err_) {
            current_tol += count[j];
            count[j] = 0;
        }
        // We add back the mistake count to the most likely category
        size_t most_likely_category = 0;
        for (size_t j = 0; j < count.size(); j++ )
        if (count[j] > count[most_likely_category])
            most_likely_category = j;
        count[most_likely_category] += current_tol;

        // Quantization
        Quantization(&prob, count, quant_const);     

        // Update model cost
        for (size_t j = 0; j < count.size(); j++ )
        if (count[j] > 0) {
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
    return table_size * (target_range_ - 1) * cell_size_ 
            + predictor_list_.size() * 16 
            + predictor_range_.size() * 16 + 40;
}

void TableCategorical::WriteModel(ByteWriter* byte_writer,
                                  size_t block_index) const {
    // Write Model Description Prefix
    byte_writer->WriteByte(Model::TABLE_CATEGORY, block_index);
    byte_writer->WriteByte(predictor_list_.size(), block_index);
    byte_writer->WriteByte(cell_size_, block_index);
    for (size_t i = 0; i < predictor_list_.size(); ++i )
        byte_writer->Write16Bit(predictor_list_[i], block_index);
    for (size_t i = 0; i < predictor_range_.size(); ++i )
        byte_writer->Write16Bit(predictor_range_[i], block_index);
    byte_writer->Write16Bit(target_range_, block_index);

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
        std::vector<double> prob_segs = dynamic_list_[predictors];
        prob_segs.resize(target_range_ - 1);
        for (size_t j = 0; j < prob_segs.size(); ++j ) 
        if (cell_size_ == 16) {
            byte_writer->Write16Bit((int)round(prob_segs[j] * 65535), block_index);
        } else {
            byte_writer->WriteByte((int)round(prob_segs[j] * 255), block_index);
        }
    }
}

Model* TableCategorical::ReadModel(ByteReader* byte_reader, const Schema& schema, size_t index) {
    size_t predictor_size = byte_reader->ReadByte();
    size_t cell_size = byte_reader->ReadByte();
    std::vector<size_t> predictor_list;
    for (size_t i = 0; i < predictor_size; ++i ) {
        size_t pred = byte_reader->Read16Bit();
        predictor_list.push_back(pred);
    }
    // err is 0 because it is only used in training
    TableCategorical* model = new TableCategorical(schema, predictor_list, index, 0);
    
    for (size_t i = 0; i < predictor_size; ++i ) {
        size_t pred_size = byte_reader->Read16Bit(); 
        model->predictor_range_[i] = pred_size;
    }
    size_t target_range = byte_reader->Read16Bit();
    model->target_range_ = target_range;

    // Read Model Parameters
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
        std::vector<double>& prob_segs = model->dynamic_list_[predictors];
        prob_segs.resize(target_range - 1);
        for (size_t j = 0; j < prob_segs.size(); ++j ) 
        if (cell_size == 16) {
            prob_segs[j] = (double)byte_reader->Read16Bit() / 65535;
        } else {
            prob_segs[j] = (double)byte_reader->ReadByte() / 255;
        }
    }
    
    return model;
}

}  // namespace db_compress
