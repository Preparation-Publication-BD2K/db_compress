#include "model.h"

#include "attribute.h"
#include "base.h"
#include "categorical_model.h"
#include "gaussian_model.h"
#include "string_model.h"

#include <vector>
#include <set>
#include <map>
#include <memory>

namespace db_compress {

namespace {

// Caller takes ownership
Model* CreateModel(const Schema& schema, const std::vector<size_t>& predictors, 
                    size_t target_var, const CompressionConfig& config) {
    Model* ret;
    double err = config.allowed_err[target_var];
    switch (GetBaseType(schema.attr_type[target_var])) {
      case BASE_TYPE_INTEGER:
        ret = new TableGaussian(schema, predictors, target_var, true, err);
        break;
      case BASE_TYPE_DOUBLE:
        ret = new TableGaussian(schema, predictors, target_var, false, err);
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

}  // anonymous namespace

int ModelLearner::GetModelCost(const Model& model) const {
    std::set<size_t> predictors(model.GetPredictorList().begin(), model.GetPredictorList().end());
    size_t target = model.GetTargetVar();
    auto it = stored_model_cost_.find(make_pair(predictors, target));
    if (it == stored_model_cost_.end())
        return -1;
    else
        return it->second;
}


void ModelLearner::StoreModelCost(const Model& model) {
    std::set<size_t> predictors(model.GetPredictorList().begin(), model.GetPredictorList().end());
    size_t target = model.GetTargetVar();
    stored_model_cost_[make_pair(predictors, target)] = model.GetModelCost();
}


ModelLearner::ModelLearner(const Schema& schema, const CompressionConfig& config) :
    schema_(schema),
    config_(config),
    stage_(0) {
    if (config_.sort_by_attr != -1) {
        ordered_attr_list_.push_back(config_.sort_by_attr);
        model_predictor_list_.push_back(std::vector<size_t>());
    }
    InitModelList();
}

const std::vector<size_t>& ModelLearner::GetOrderOfAttributes() const {
    return ordered_attr_list_;
}

void ModelLearner::FeedTuple(const Tuple& tuple) {
    for (size_t i = 0; i < active_model_list_.size(); i++ )
        active_model_list_[i]->FeedTuple(tuple);
}

bool ModelLearner::RequireFullPass() const {
    return (stage_ != 0);
}

bool ModelLearner::RequireMoreIterations() const {
    return (stage_ != 2);
}

void ModelLearner::EndOfData() {
    switch (stage_) {
      case 0:
        // At the end of data, we inform each of the active models, let them compute their
        // model cost, and then store them into the stored_model_cost_ variable.
        for (size_t i = 0; i < active_model_list_.size(); i++ )
            active_model_list_[i]->EndOfData();
        for (size_t i = 0; i < active_model_list_.size(); i++ )
            StoreModelCost(*active_model_list_[i]);
        ExpandModelList();
        
        // Now if there is no longer any active model, we add the best model to ordered_attr_list_
        // and then start a new iteration. Note that in order to save memory space, we only store
        // the target variable and predictor variables, the actual model will be learned again
        // during the second stage of the algorithm. We use while loop here because active
        // model list can be empty immediately after InitModelList().
        while (active_model_list_.size() == 0) {
            Model* best_model = model_list_[0].get();
            for (size_t i = 0; i < model_list_.size(); i++ )
            if (GetModelCost(*model_list_[i]) < GetModelCost(*best_model))
                best_model = model_list_[i].get();
            ordered_attr_list_.push_back(best_model->GetTargetVar());
            model_predictor_list_.push_back(best_model->GetPredictorList());
            
            // Now if we reach the point where models for every attribute has been selected, 
            // we mark the end of this stage and start next stage. Otherwise we simply start
            // another iteration.
            if (ordered_attr_list_.size() == schema_.attr_type.size())
                stage_ = 1;
            InitModelList();
        }
        break;
      case 1:
        selected_model_.resize(schema_.attr_type.size());
        for (size_t i = 0; i < schema_.attr_type.size(); i++ ) {
            active_model_list_[i]->EndOfData();
            int targetVar = active_model_list_[i]->GetTargetVar();
            selected_model_[targetVar] = std::move(active_model_list_[i]);
        }
        stage_ = 2;
    }
}

void ModelLearner::InitModelList() {
    model_list_.clear();
    active_model_list_.clear();

    if (stage_ == 0) {
        // In the first stage, we initially create an empty model for every inactive attribute.
        // Then we expand each of these models.
        std::set<size_t> inactive_attr(ordered_attr_list_.begin(), ordered_attr_list_.end());
        for (size_t i = 0; i < schema_.attr_type.size(); i++ )
        if (inactive_attr.count(i) == 0) {
            std::unique_ptr<Model> model(CreateModel(schema_, std::vector<size_t>(), i, config_));
            if (GetModelCost(*model) == -1) {
                active_model_list_.push_back(std::move(model));
            } else
                model_list_.push_back(std::move(model));
        }
        ExpandModelList();
    } else {
        // In the second stage, we simply relearn the model selected from the first stage,
        // no model expansion is needed.
        for (size_t i = 0; i < schema_.attr_type.size(); i++ ) {
            std::unique_ptr<Model> ptr(
                CreateModel(schema_, model_predictor_list_[i], ordered_attr_list_[i], config_)
            );
            active_model_list_.push_back(std::move(ptr));
        }   
    }
}

void ModelLearner::ExpandModelList() {
    std::vector< std::unique_ptr<Model> > best_model_list_(schema_.attr_type.size());
    for (size_t i = 0; i < active_model_list_.size(); i++)
        model_list_.push_back(std::move(active_model_list_[i]));
    active_model_list_.clear();
    for (size_t i = 0; i < model_list_.size(); i++ ) {
        std::unique_ptr<Model> model = std::move(model_list_[i]);
        if (GetModelCost(*model) == -1) {
            active_model_list_.push_back(std::move(model));
        } else {
            if (!best_model_list_[model->GetTargetVar()])
                best_model_list_[model->GetTargetVar()] = std::move(model);
            else {
                Model* previous = best_model_list_[model->GetTargetVar()].get();
                if (GetModelCost(*previous) > GetModelCost(*model)) {
                    best_model_list_[model->GetTargetVar()] = std::move(model);
                }   
            }
        }
    }
    model_list_.clear();
    // If there are models that predicts attribute i, then we select the best model among
    // them, and expand this model, if all expanded models are inactive, we can continue the
    // expansion process. If all expanded models are no better than the original model, we stop
    // expansion.
    for (size_t i = 0; i < schema_.attr_type.size(); i++) 
    while (best_model_list_[i]) {
        std::vector<size_t> predictor_list(best_model_list_[i]->GetPredictorList());
        std::set<size_t> predictor_set(predictor_list.begin(), predictor_list.end());
        predictor_list.push_back(0);
        bool new_active_model = false;
        bool best_model_expanded = false;
        for (size_t attr : ordered_attr_list_) { 
            if (predictor_set.count(attr) == 0) {
                predictor_list[predictor_set.size()] = attr;
                // Here we assume that each (predictor, target) pair is associated
                // with exactly one model.
                std::unique_ptr<Model> model(CreateModel(schema_, predictor_list, i, config_));
                if (GetModelCost(*model) == -1) {
                    new_active_model = true;
                    active_model_list_.push_back(std::move(model));
                } else if (GetModelCost(*model) < GetModelCost(*best_model_list_[i])) {
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

Model* ModelLearner::GetModel(size_t attr) {
    return selected_model_[attr].release();
}

}  // namespace db_compress

