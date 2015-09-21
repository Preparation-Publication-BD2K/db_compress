#include "categorical_model.h"

#include "base.h"
#include "model.h"
#include "utility.h"

#include <vector>
#include <cmath>

namespace db_compress {

namespace {

std::vector<size_t> GetPredictorCap(const Schema& schema, const std::vector<size_t>& pred) {
    std::vector<size_t> cap;
    for (size_t i = 0; i < pred.size(); ++i)
        cap.push_back(GetAttrInterpreter(schema.attr_type[pred[i]])->EnumCap());
    return cap;
}

}  // anonymous namespace

CategoricalProbTree::CategoricalProbTree(const std::vector<Prob>& prob_segs) : 
    choice_(-1) {
    prob_segs_ = prob_segs;
}

int CategoricalProbTree::GetNextBranch(const AttrValue* attr) const {
    int branch = static_cast<const EnumAttrValue*>(attr)->Value();
    ProbInterval PI = this->GetProbInterval(branch);
    if (PI.l == PI.r) {
        // We found the "omitted" categorical values. Recover it with the most likely category
        Prob current_best;
        for (size_t i = 0; i <= prob_segs_.size(); ++i ) {
            if (GetLen(this->GetProbInterval(i)) > current_best) {
                current_best = GetLen(this->GetProbInterval(i));
                branch = i;
            }
        } 
    }
    return branch;
}

TableCategorical::TableCategorical(const Schema& schema, 
                                   const std::vector<size_t>& predictor_list, 
                                   size_t target_var, 
                                   double err) :
    Model(predictor_list, target_var),
    predictor_interpreter_(predictor_list_.size()),
    target_range_(0),
    cell_size_(0),
    err_(err),
    model_cost_(0),
    dynamic_list_(GetPredictorCap(schema, predictor_list)) {
    for (size_t i = 0; i < predictor_list_.size(); ++i) {
        predictor_interpreter_[i] = GetAttrInterpreter(schema.attr_type[predictor_list[i]]);
    }
}

ProbTree* TableCategorical::GetProbTree(const Tuple& tuple) {
    std::vector<size_t> index;
    GetDynamicListIndex(tuple, &index);
    prob_tree_.reset(new CategoricalProbTree(dynamic_list_[index].prob));
    return prob_tree_.get(); 
}

void TableCategorical::GetDynamicListIndex(const Tuple& tuple, std::vector<size_t>* index) {
    index->clear();
    for (size_t i = 0; i < predictor_list_.size(); ++i ) {
        const AttrValue* attr = tuple.attr[predictor_list_[i]];
        size_t val = predictor_interpreter_[i]->EnumInterpret(attr);
        index->push_back(val);
    }
}

void TableCategorical::FeedTuple(const Tuple& tuple) {
    std::vector<size_t> predictors;
    GetDynamicListIndex(tuple, &predictors);
    const AttrValue* attr = tuple.attr[target_var_];
    size_t target_val = static_cast<const EnumAttrValue*>(attr)->Value();
    if (target_val >= target_range_)
        target_range_ = target_val + 1;
   
    CategoricalStats& vec = dynamic_list_[predictors];
    if (vec.count.size() <= target_val) {
        vec.count.resize(target_val + 1);
        vec.count.shrink_to_fit();
    }
    ++ vec.count.at(target_val);
}

void TableCategorical::EndOfData() {
    // Determine cell size
    cell_size_ = (target_range_ > 100 ? 16 : 8);
    for (size_t i = 0; i < dynamic_list_.size(); ++i ) {
        CategoricalStats& stats = dynamic_list_[i];
        // Extract count vector
        std::vector<int> count;
        count.swap(stats.count);

        // We might need to resize count vector
        count.resize(target_range_);
        std::vector<Prob> prob;

        // Mark empty entries, since we are allowed to make mistakes,
        // we can mark entries that rarely appears as empty
        int total_count = 0, current_tol = 0;
        for (size_t j = 0; j < count.size(); ++j )
            total_count += count[j];
        for (size_t j = 0; j < count.size(); ++j )
        if (count[j] + current_tol <= total_count * err_) {
            current_tol += count[j];
            count[j] = 0;
        }
        // We add back the mistake count to the most likely category
        size_t most_likely_category = 0;
        for (size_t j = 0; j < count.size(); ++j )
        if (count[j] > count[most_likely_category])
            most_likely_category = j;
        count[most_likely_category] += current_tol;

        // Quantization
        Quantization(&prob, count, cell_size_);     

        // Update model cost
        for (size_t j = 0; j < count.size(); j++ )
        if (count[j] > 0) {
            Prob p = (j == count.size() - 1 ? GetOneProb() : prob[j])
                        - (j == 0 ? GetZeroProb() : prob[j - 1]);
            model_cost_ += count[j] * (- log2(CastDouble(p)) ); 
        }
        // Write back to dynamic list
        stats.prob = prob;
    }
    // Add model description length to model cost
    model_cost_ += GetModelDescriptionLength();
}

int TableCategorical::GetModelDescriptionLength() const {
    size_t table_size = dynamic_list_.size();
    // See WriteModel function for details of model description.
    return table_size * (target_range_ - 1) * cell_size_ 
            + predictor_list_.size() * 16 + 32;
}

void TableCategorical::WriteModel(ByteWriter* byte_writer,
                                  size_t block_index) const {
    // Write Model Description Prefix
    byte_writer->WriteByte(predictor_list_.size(), block_index);
    byte_writer->WriteByte(cell_size_, block_index);
    for (size_t i = 0; i < predictor_list_.size(); ++i )
        byte_writer->Write16Bit(predictor_list_[i], block_index);
    byte_writer->Write16Bit(target_range_, block_index);

    // Write Model Parameters
    size_t table_size = dynamic_list_.size();
    for (size_t i = 0; i < table_size; ++i ) {
        std::vector<Prob> prob_segs = dynamic_list_[i].prob;
        for (size_t j = 0; j < prob_segs.size(); ++j ) {
            int code = CastInt(prob_segs[j], cell_size_);
            if (cell_size_ == 16) {
                byte_writer->Write16Bit(code, block_index);
            } else {
                byte_writer->WriteByte(code, block_index);
            }
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
    // set err to 0 because err is only used in training
    TableCategorical* model = new TableCategorical(schema, predictor_list, index, 0);
    
    size_t target_range = byte_reader->Read16Bit();
    model->target_range_ = target_range;

    // Read Model Parameters
    size_t table_size = model->dynamic_list_.size();
    for (size_t i = 0; i < table_size; ++i ) {
        std::vector<Prob>& prob_segs = model->dynamic_list_[i].prob;
        prob_segs.resize(target_range - 1);
        for (size_t j = 0; j < prob_segs.size(); ++j ) 
        if (cell_size == 16) { 
            prob_segs[j] = GetProb(byte_reader->Read16Bit(), 16);
        } else {
            prob_segs[j] = GetProb(byte_reader->ReadByte(), 8);
        }
    }
    
    return model;
}

Model* TableCategoricalCreator::ReadModel(ByteReader* byte_reader, 
                                               const Schema& schema, size_t index) {
    return TableCategorical::ReadModel(byte_reader, schema, index);
}

Model* TableCategoricalCreator::CreateModel(const Schema& schema,
            const std::vector<size_t>& predictor, size_t index, double err) {
    for (size_t i = 0; i < predictor.size(); ++i) {
        int attr_type = schema.attr_type[predictor[i]];
        if (!GetAttrInterpreter(attr_type)->EnumInterpretable())
            return NULL;
    }
    return new TableCategorical(schema, predictor, index, err);
}

}  // namespace db_compress
