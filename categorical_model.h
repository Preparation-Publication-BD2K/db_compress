#ifndef CATEGORICAL_MODEL_H
#define CATEGORICAL_MODEL_H

#include "model.h"
#include "base.h"
#include "utility.h"

#include <vector>

namespace db_compress {

class CategoricalProbDist : public ProbDist {
  private:
  public:
    bool End();
    void FeedByte(char byte);
    int GetUnusedBits();
    ProbInterval GetRemainProbInterval();
    AttrValue* GetResult();
    void Reset();
};

class TableCategorical : public Model {
  private:
    std::vector<size_t> predictor_list_;
    std::vector<size_t> predictor_range_;
    size_t target_var_;
    size_t target_range_;
    size_t cell_size_;
    double err_;
    double model_cost_;
    // Each vector consists of k-1 probability segment boundary
    DynamicList<std::vector<double>> dynamic_list_;
    void GetDynamicListIndex(const Tuple& tuple, std::vector<size_t>* index);
    static std::vector<size_t> GetPredictorList(const Schema& schema, 
                                                const std::vector<size_t>& predictor_list);
   
  public:
    TableCategorical(const Schema& schema, const std::vector<size_t>& predictor_list, 
                    size_t target_var, double err);
    ProbDist* GetProbDist(const Tuple& tuple, const ProbInterval& prob_interval);
    void GetProbInterval(const Tuple& tuple, std::vector<ProbInterval>* prob_intervals,
                                 std::unique_ptr<AttrValue>* result_attr);
    const std::vector<size_t>& GetPredictorList() const;
    size_t GetTargetVar() const;
    int GetModelCost() const;
    void FeedTuple(const Tuple& tuple);
    void EndOfData();

    int GetModelDescriptionLength() const;
    void WriteModel(ByteWriter* byte_writer, size_t block_index) const;
};

} // namespace db_compress

#endif
