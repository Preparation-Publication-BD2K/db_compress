#include "categorical_model.h"

#include "attribute.h"
#include "base.h"
#include "model.h"
#include "utility.h"

#include <vector>
#include <cmath>

namespace db_compress {

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

ProbDist* TableCategorical::GetProbDist(const Tuple& tuple, 
                                        const ProbInterval& prob_interval) {
    //Todo:
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

ProbInterval TableCategorical::GetProbInterval(const Tuple& tuple, 
                                               const ProbInterval& prob_interval, 
                                               std::vector<unsigned char>* emit_bytes,
                                               std::unique_ptr<AttrValue>* result_attr) {
    std::vector<size_t> predictors;
    GetDynamicListIndex(tuple, &predictors);
    AttrValue* attr = tuple.attr[target_var_].get();
    size_t target_val = static_cast<EnumAttrValue*>(attr)->Value();

    const std::vector<double>& vec = dynamic_list_[predictors];
    double l = (target_val == 0 ? 0 : vec[target_val - 1]);
    double r = (target_val == vec.size() ? 1 : vec[target_val]);
    if (r - l < 1e-9) {
        // We found the "omitted" categorical values. Recover it with the most likely category
        for (size_t i = 0; i <= vec.size(); i++ ) {
            double new_l = (i == 0 ? 0 : vec[i - 1]);
            double new_r = (i == vec.size() ? 1 : vec[i]);
            if (new_r - new_l > r - l) {
                l = new_l;
                r = new_r;
            }
        }
    }
    
    if (emit_bytes != NULL)
        emit_bytes->clear();
    ProbInterval ret(0, 1);
    GetProbSubinterval(prob_interval.l, prob_interval.r, l, r, &ret.l, &ret.r, emit_bytes);
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
            + predictor_list_.size() * 8 
            + predictor_range_.size() * 8 + 24;
}

void TableCategorical::WriteModel(ByteWriter* byte_writer,
                                  size_t block_index) const {
    // Write Model Description Prefix
    byte_writer->WriteByte(Model::TABLE_CATEGORY, block_index);
    byte_writer->WriteByte(predictor_list_.size(), block_index);
    byte_writer->WriteByte(cell_size_, block_index);
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
        for (size_t j = 0; j < prob_segs.size(); j++ ) 
        if (cell_size_ == 16) {
            int code = round(prob_segs[j] * 65535);
            byte_writer->WriteByte(code / 256, block_index);
            byte_writer->WriteByte(code & 255, block_index);
        } else {
            byte_writer->WriteByte((int)round(prob_segs[j] * 255), block_index);
        }
    }
}

}  // namespace db_compress
