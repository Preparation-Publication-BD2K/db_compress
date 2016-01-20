#ifndef MODEL_LEARNER_H
#define MODEL_LEARNER_H

#include "base.h"
#include "data_io.h"
#include "model.h"

#include <vector>
#include <set>
#include <map>
#include <memory>

namespace db_compress {

struct CompressionConfig {
    std::vector<double> allowed_err;
    // sort_by_attr = -1 means no specific sorting required
    int sort_by_attr;
    bool skip_model_learning;
};

/*
 * The ModelLearner class learns all the models simultaneously in an online fashion.
 */
class ModelLearner {
  private:
    Schema schema_;
    CompressionConfig config_;
    int stage_;
    std::vector<size_t> ordered_attr_list_;
    std::set<size_t> inactive_attr_;
    std::vector< std::unique_ptr<Model> > active_model_list_;
    std::vector< std::unique_ptr<Model> > selected_model_;
    std::vector< std::vector<size_t> > model_predictor_list_;
    std::map< std::pair<std::set<size_t>, size_t>, int> stored_model_cost_;
    
    void InitActiveModelList();
    void StoreModelCost(const Model& model);
    // Get the model cost based on predictors and target variable.
    // If not known, return -1
    int GetModelCost(const std::vector<size_t>& predictor, size_t target) const;
  public:
    ModelLearner(const Schema& schema, const CompressionConfig& config);
    // These functions are used to learn the Model objects.
    void FeedTuple(const Tuple& tuple);
    bool RequireFullPass() const { return stage_ != 0; }
    bool RequireMoreIterations() const { return stage_ != 2; }
    void EndOfData();
    // This function gets the Model object for any particular attribute. Caller takes
    // ownership of the Model object. 
    Model* GetModel(size_t attr_index) { return selected_model_[attr_index].release(); }
    // This function gets the order of attributes during the encoding/decoding phase
    const std::vector<size_t>& GetOrderOfAttributes() const { return ordered_attr_list_; }
};

} // namespace db_compress

#endif
