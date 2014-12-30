#include "model.h"
#include "model_impl.h"
#include "attribute.h"

#include <vector>
#include <set>
#include <memory>

namespace db_compress {

namespace {

int GetModelCost(const ModelLearner& learner, const Model& model) {
// Check the SET of predictor list, not List
}

// Caller takes ownership
Model* CreateModel(const Schema& schema, const std::vector<int>& predictors, int target_var, const CompressionConfig& config) {
    Model* ret;
    double err = config.allowed_err[target_var];
    switch (GetBaseType(schema.attr_type[target_var])) {
      case BASE_TYPE_INTEGER:
        ret = new TableGuassian(schema, predictors, target_var, true, err);
        break;
      case BASE_TYPE_DOUBLE:
        ret = new TableGuassian(schema, predictors, target_var, false, err);
        break;
      case BASE_TYPE_STRING:
        ret = new StringModel(target_var);
        break;
      case BASE_TYPE_ENUM:
        ret = new TableCategorical(schema, predictors, target_var, err);
        break;
    }
    return ret;
}

void StoreModelCost(ModelLearner* learner, const Model& model) {
}

}  // anonymous namespace

ModelLearner::ModelLearner(const Schema& schema, const CompressionConfig& config) :
    schema_(schema),
    config_(config),
    stage_(0) {
    if (config_.sort_by_attr != -1) {
        ordered_attr_list_.push_back(config_.sort_by_attr);
    }
    InitModelList();
}

const std::vector<int>& ModelLearner::GetOrderOfAttributes() const {
    return ordered_attr_list_;
}

void ModelLearner::FeedTuple(const Tuple& tuple) {
    for (int i = 0; i < active_model_list_.size(); i++ )
        active_model_list_[i]->FeedTuple(tuple);
}

bool ModelLearner::RequireMoreIterations() const {
    return (stage_ != 2);
}

void ModelLearner::EndOfData() {
    switch (stage_) {
      case 0:
        for (int i = 0; i < active_model_list_.size(); i++ )
            active_model_list_[i]->EndOfData();
        for (int i = 0; i < active_model_list_.size(); i++ )
            StoreModelCost(this, *active_model_list_[i]);
        ExpandModelList();
        if (active_model_list_.size() == 0) {
            Model* best_model = model_list_[0].get();
            for (int i = 0; i < model_list_.size(); i++ )
            if (GetModelCost(*this, *model_list_[i]) < GetModelCost(*this, *best_model))
                best_model = model_list_[i].get();
            ordered_attr_list_.push_back(best_model->GetTargetVar());
            model_predictor_list_.push_back(best_model->GetPredictorList());
            if (ordered_attr_list_.size() == schema_.attr_type.size()) {
                active_model_list_.clear();
                model_list_.clear();
                for (int i = 0; i < schema_.attr_type.size(); i++ ) {
                    std::unique_ptr<Model> ptr(
                        CreateModel(schema_, model_predictor_list_[i], ordered_attr_list_[i], config_));
                    active_model_list_.push_back(std::move(ptr));
                }   
                stage_ = 1;
            } else {
                InitModelList();
                ExpandModelList();
            }
        }
        break;
      case 1:
        selected_model_.resize(schema_.attr_type.size());
        for (int i = 0; i < schema_.attr_type.size(); i++ ) {
            int targetVar = active_model_list_[i]->GetTargetVar();
            selected_model_[targetVar] = std::move(active_model_list_[i]);
        }
        stage_ = 2;
    }
}

void ModelLearner::InitModelList() {
    model_list_.clear();
    active_model_list_.clear();

    std::set<int> inactive_attr(ordered_attr_list_.begin(), ordered_attr_list_.end());
    for (int i = 0; i < schema_.attr_type.size(); i++ )
    if (inactive_attr.count(i) == 0) {
        std::unique_ptr<Model> model(CreateModel(schema_, std::vector<int>(), 0, config_));
        if (GetModelCost(*this, *model) == -1) {
            active_model_list_.push_back(std::move(model));
        } else
            model_list_.push_back(std::move(model));
    }
}

void ModelLearner::ExpandModelList() {
    std::vector< std::unique_ptr<Model> > best_model_list_(schema_.attr_type.size());
    for (int i = 0; i < active_model_list_.size(); i++)
        model_list_.push_back(std::move(active_model_list_[i]));
    active_model_list_.clear();
    for (int i = 0; i < model_list_.size(); i++ ) {
        std::unique_ptr<Model> model = std::move(model_list_[i]);
        if (GetModelCost(*this, *model) == -1) {
            active_model_list_.push_back(std::move(model));
        } else {
            if (!best_model_list_[model->GetTargetVar()])
                best_model_list_[model->GetTargetVar()] = std::move(model);
            else {
                Model* previous = best_model_list_[model->GetTargetVar()].get();
                if (GetModelCost(*this, *previous) > GetModelCost(*this, *model)) {
                    best_model_list_[model->GetTargetVar()] = std::move(model);
                }   
            }
        }
    }
    model_list_.clear();
    // If there are models that predicts attribute i, then we expand the best model among
    // them, and expand this model, if we can guarantee the expanded model is still optimal
    // then we can continue expanding it.
    for (int i = 0; i < schema_.attr_type.size(); i++) 
    while (best_model_list_[i]) {
        std::vector<int> predictor_list(best_model_list_[i]->GetPredictorList());
        std::set<int> predictor_set(predictor_list.begin(), predictor_list.end());
        predictor_list.push_back(0);
        bool new_active_model = false;
        bool best_model_expanded = false;
        for (int attr : ordered_attr_list_) { 
            if (predictor_set.count(attr) == 0) {
                predictor_list[predictor_set.size()] = attr;
                // Here we assume that each base type is associated with exactly one model.
                // If there are more than one model, we can store them in vector.
                std::unique_ptr<Model> model(CreateModel(schema_, predictor_list, i, config_));
                if (GetModelCost(*this, *model) == -1) {
                    new_active_model = true;
                    active_model_list_.push_back(std::move(model));
                } else if (GetModelCost(*this, *model) < GetModelCost(*this, *best_model_list_[i])) {
                    best_model_list_[i] = std::move(model);
                    best_model_expanded = true;
                }
            }
        }
        if (new_active_model || (!best_model_expanded)) {
            // In this case we do not continue expanding the model
            model_list_.push_back(std::move(best_model_list_[i]));
            best_model_list_[i] = NULL;
        }
    }
}

Model* ModelLearner::GetModel(int attr) {
    return selected_model_[attr].release();
}

}  // namespace db_compress

