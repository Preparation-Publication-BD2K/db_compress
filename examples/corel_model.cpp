#include "corel.h"
#include "../base.h"
#include "../numerical_model.h"
#include "../utility.h"

#include <memory>
#include <vector>

ColorSquID::ColorSquID(db_compress::Prob zero_prob,
                       db_compress::LaplaceSquID* tree) :
    first_step_(true),
    is_zero_(false),
    zero_prob_(zero_prob),
    tree_(tree),
    attr_(0) {}

bool ColorSquID::HasNextBranch() const {
    if (first_step_)
        return true;
    else if (is_zero_)
        return false;
    else
        return tree_->HasNextBranch();
}

void ColorSquID::GenerateNextBranch() {
    if (first_step_) {
        prob_segs_.clear();
        prob_segs_.push_back(zero_prob_);
    } else {
        tree_->GenerateNextBranch();
        prob_segs_ = tree_->GetProbSegs();
    }
}

int ColorSquID::GetNextBranch(const db_compress::AttrValue* attr) const {
    double value = static_cast<const ColorAttr*>(attr)->Value();
    if (first_step_) {
        if (fabs(value) < 1e-5)
            return 0;
        else
            return 1;
    } else {
        db_compress::DoubleAttrValue attr_(value);
        return tree_->GetNextBranch(&attr_);
    }
}

void ColorSquID::ChooseNextBranch(int branch) {
    if (first_step_) {
        if (branch == 0)
            is_zero_ = true;
        first_step_ = false;
    } else
        tree_->ChooseNextBranch(branch);
}

const db_compress::AttrValue* ColorSquID::GetResultAttr() {
    if (is_zero_) {
        attr_.Set(0);
    } else {
        const db_compress::AttrValue* ptr(tree_->GetResultAttr());
        double value = static_cast<const db_compress::DoubleAttrValue*>(ptr)->Value();
        attr_.Set(value);
    }
    return &attr_;
}

ColorModel::ColorModel(const db_compress::Schema& schema, const std::vector<size_t>& predictors,
                       size_t target_var, double err) :
    SquIDModel(predictors, target_var),
    numeric_model_(new db_compress::TableLaplace(schema, predictors, target_var, err, false)),
    zero_count_(0),
    non_zero_count_(0),
    model_cost_(0) {}

db_compress::SquID* ColorModel::GetSquID(const db_compress::Tuple& tuple) {
    db_compress::LaplaceSquID* tree_ = 
        static_cast<db_compress::LaplaceSquID*>(numeric_model_->GetSquID(tuple));
    squid_.reset(new ColorSquID(zero_prob_, tree_));
    return squid_.get();
}

void ColorModel::FeedTuple(const db_compress::Tuple& tuple) {
    double value = static_cast<const ColorAttr*>(tuple.attr[target_var_])->Value();
    if (fabs(value) < 1e-5)
        ++ zero_count_;
    else {
        ++ non_zero_count_;
        numeric_model_->FeedTuple(tuple);
    }
}

void ColorModel::EndOfData() {
    numeric_model_->EndOfData();

    std::vector<int> cnt;
    cnt.push_back(zero_count_);
    cnt.push_back(non_zero_count_);
    std::vector<db_compress::Prob> vec;
    db_compress::Quantization(&vec, cnt, 16);
    zero_prob_ = vec[0];

    model_cost_ = numeric_model_->GetModelCost();
}

int ColorModel::GetModelDescriptionLength() const {
    return 16 + numeric_model_->GetModelDescriptionLength();
}

void ColorModel::WriteModel(db_compress::ByteWriter* byte_writer, size_t block_index) const {
    numeric_model_->WriteModel(byte_writer, block_index);
    byte_writer->Write16Bit(CastInt(zero_prob_, 16), block_index);
}

ColorModel* ColorModel::ReadModel(db_compress::ByteReader* byte_reader,
                                  const db_compress::Schema& schema, size_t index) {
    db_compress::SquIDModel* model = 
        db_compress::TableLaplace::ReadModel(byte_reader, schema, index, false);
    ColorModel* ret = new ColorModel(schema, model->GetPredictorList(), index, 0);
    ret->numeric_model_.reset(model);
    ret->zero_prob_ = db_compress::GetProb(byte_reader->Read16Bit(), 16);
    return ret; 
}

db_compress::SquIDModel* ColorModelCreator::ReadModel(db_compress::ByteReader* byte_reader,
                                                 const db_compress::Schema& schema, size_t index) {
    return ColorModel::ReadModel(byte_reader, schema, index);
}

db_compress::SquIDModel* ColorModelCreator::CreateModel(const db_compress::Schema& schema,
                                                   const std::vector<size_t>& predictors, 
                                                   size_t target, double err) {
    for (size_t i = 0; i < predictors.size(); ++i) {
        int attr_type = schema.attr_type[predictors[i]];
        if (!db_compress::GetAttrInterpreter(attr_type)->EnumInterpretable())
            return NULL;
    }
    return new ColorModel(schema, predictors, target, err);
}

int ColorInterpreter::EnumCap() const { return 5; }

int ColorInterpreter::EnumInterpret(const db_compress::AttrValue* attr) const {
    double value = static_cast<const ColorAttr*>(attr)->Value();
    if (value < 0.01) return 0;
    if (value < 0.25) return 1;
    if (value < 0.5) return 2;
    if (value < 0.75) return 3;
    return 4;
}
