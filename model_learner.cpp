#include "model_learner.h"

#include "base.h"
#include "model.h"

#include <vector>
#include <set>
#include <map>
#include <memory>

namespace db_compress {

namespace {

// New Models are appended to the end of vector
void CreateModel(const Schema& schema, const std::vector<size_t>& predictors, 
                 size_t target_var, const CompressionConfig& config, 
                 std::vector<std::unique_ptr<Model> >* vec) {
    double err = config.allowed_err[target_var];
    int attr_type = schema.attr_type[target_var];
    const std::vector<ModelCreator*>& creators = GetAttrModel(attr_type);
    for (size_t i = 0; i < creators.size(); ++i) {
        ModelCreator* creator = creators[i];
        std::unique_ptr<Model> model(creator->CreateModel(schema, predictors, target_var, err));
        model->SetCreatorIndex(i);
        if (model != nullptr)
            vec->push_back(std::move(model));
    }
}

}  // anonymous namespace

int ModelLearner::GetModelCost(const std::vector<size_t>& predictors, size_t target) const {
    std::set<size_t> predictors_(predictors.begin(), predictors.end());
    auto it = stored_model_cost_.find(make_pair(predictors_, target));
    if (it == stored_model_cost_.end())
        return -1;
    else
        return it->second;
}


void ModelLearner::StoreModelCost(const Model& model) {
    std::set<size_t> predictors(model.GetPredictorList().begin(), model.GetPredictorList().end());
    size_t target = model.GetTargetVar();
    int previous_cost = GetModelCost(model.GetPredictorList(), model.GetTargetVar());
    if (previous_cost == -1 || previous_cost > model.GetModelCost())
        stored_model_cost_[make_pair(predictors, target)] = model.GetModelCost();
}


ModelLearner::ModelLearner(const Schema& schema, const CompressionConfig& config) :
    schema_(schema),
    config_(config),
    stage_(0),
    selected_model_(schema.attr_type.size()),
    model_predictor_list_(schema.attr_type.size()) {
    if (config_.sort_by_attr != -1) {
        ordered_attr_list_.push_back(config_.sort_by_attr);
        inactive_attr_.insert(config_.sort_by_attr);
        model_predictor_list_[config_.sort_by_attr].clear();
    }
    InitActiveModelList();
}

void ModelLearner::FeedTuple(const Tuple& tuple) {
    switch (stage_) {
      case 0:
        for (size_t i = 0; i < active_model_list_.size(); ++i )
            active_model_list_[i]->FeedTuple(tuple);
        break;
      case 1:
        {
            Tuple tuple_ = tuple;
            std::vector<std::unique_ptr<AttrValue> > vec;
            for (size_t i = 0; i < schema_.attr_type.size(); ++i ) {
                size_t attr_index = ordered_attr_list_[i];
                // Since decoding is lossy, we have to use the predicted predictors
                // instead of the original predictors during this phase of training
                if (inactive_attr_.count(attr_index) > 0) {
                    std::unique_ptr<AttrValue> attr(nullptr);
                    selected_model_[attr_index]->GetProbInterval(tuple_, NULL, &attr);
                    tuple_.attr[attr_index] = attr.get();
                    vec.push_back(std::move(attr));
                }
            }
            for (size_t i = 0; i < active_model_list_.size(); ++i )
                active_model_list_[i]->FeedTuple(tuple_);
        }
    } 
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
        
        // Now if there is no longer any active model, we add the best model to ordered_attr_list_
        // and then start a new iteration. Note that in order to save memory space, we only store
        // the target variable and predictor variables, the actual model will be learned again
        // during the second stage of the algorithm. 
        if (active_model_list_.size() == 0) {
            int next_attr = -1;
            for (size_t i = 0; i < schema_.attr_type.size(); ++i)
            if (inactive_attr_.count(i) == 0) {
                if (next_attr == -1)
                    next_attr = i;
                else if (GetModelCost(model_predictor_list_[i], i) <
                         GetModelCost(model_predictor_list_[next_attr], next_attr))
                    next_attr = i;
            }
            ordered_attr_list_.push_back(next_attr);
            inactive_attr_.insert(next_attr);
            
            // Now if we reach the point where models for every attribute has been selected, 
            // we mark the end of this stage and start next stage. Otherwise we simply start
            // another iteration.
            if (ordered_attr_list_.size() == schema_.attr_type.size()) {
                stage_ = 1;
                inactive_attr_.clear();
            }
        }
        break;
      case 1:
        for (size_t i = 0; i < active_model_list_.size(); ++i) {
            active_model_list_[i]->EndOfData();
            int target_var = active_model_list_[i]->GetTargetVar();
            inactive_attr_.insert(target_var);
            if (selected_model_[target_var] == nullptr ||
                selected_model_[target_var]->GetModelCost() >
                active_model_list_[i]->GetModelCost() )
            selected_model_[target_var] = std::move(active_model_list_[i]);
        }
        if (inactive_attr_.size() == schema_.attr_type.size())
            stage_ = 2;
    }
    // If we still haven't reached end stage, init active models
    if (stage_ != 2)
        InitActiveModelList();
}

void ModelLearner::InitActiveModelList() {
    active_model_list_.clear();

    if (stage_ == 0) {
        // In the first stage, we initially create an empty model for every inactive attribute.
        // Then we expand each of these models.
        for (size_t i = 0; i < schema_.attr_type.size(); ++i )
        if (inactive_attr_.count(i) == 0) {
            if (GetModelCost(std::vector<size_t>(), i) == -1) {
                CreateModel(schema_, std::vector<size_t>(), i, config_, &active_model_list_);
            } else {
                // We empty the current predictor list, and search from scratch
                model_predictor_list_[i].clear();
                // We choose the models based on a greedy criterion, we choose the predictor 
                // attribute that can reduce the cost in largest amount. During this process, 
                // all models with unknown cost (a.k.a. "active" models) are added to a list, 
                // and then choose the "inactive" model with lowest cost to expand.
                while (1) {
                    std::vector<size_t> predictor_list(model_predictor_list_[i]);
                    std::set<size_t> predictor_set(predictor_list.begin(), predictor_list.end());
                    int previous_cost = GetModelCost(predictor_list, i);
                    // Add a new slot in predictor list
                    predictor_list.push_back(0);
                    bool model_expanded = false;
                    for (size_t attr : ordered_attr_list_)
                    if (predictor_set.count(attr) == 0) {
                        predictor_list[predictor_set.size()] = attr;
                        if (GetModelCost(predictor_list, i) == -1) {
                            // Multiple models may be associated for any predictor and target
                            CreateModel(schema_, predictor_list, 
                                        i, config_, &active_model_list_);
                        } else if (GetModelCost(predictor_list, i) < previous_cost) {
                            model_predictor_list_[i] = predictor_list;
                            previous_cost = GetModelCost(predictor_list, i);
                            model_expanded = true;
                        }
                    }
                    // If current model can not be expanded, break the loop
                    if (!model_expanded) break;
                }
            }
        }
    } else {
        // In the second stage, we simply relearn the model selected from the first stage,
        // no model expansion is needed. However, we need to assure that the models that are
        // currently learning have predictors all lies within the range of target vars of 
        // learned models.
        for (size_t i = 0; i < schema_.attr_type.size(); ++i ) {
            bool learnable = true;
            for (size_t attr : model_predictor_list_[i])
            if (inactive_attr_.count(attr) == 0)
                learnable = false;
            if (!learnable) continue;
            CreateModel(schema_, model_predictor_list_[i], i, config_, &active_model_list_);
        }   
    }
}

}  // namespace db_compress

