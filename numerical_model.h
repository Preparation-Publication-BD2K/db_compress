#ifndef NUMERICAL_MODEL_H
#define NUMERICAL_MODEL_H

#include "model.h"
#include "base.h"
#include "utility.h"

#include <vector>

namespace db_compress {

struct LaplaceStats {
    int count;
    double median;
    double sum_abs_dev;
    double mean_abs_dev;
    std::vector<double> values;
    LaplaceStats() : count(0), median(0), sum_abs_dev(0) {}
    void GetMedian();
};

class LaplaceProbDist : public ProbDist {
  private:
  public:
    bool End();
    void FeedByte(char byte);
    int GetUnusedBits();
    ProbInterval GetRemainProbInterval();
    AttrValue* GetResult();
    void Reset();
};

class TableLaplace : public Model {
  private:
    std::vector<size_t> predictor_list_;
    std::vector<size_t> predictor_range_;
    size_t target_var_;
    double err_;
    bool target_int_;
    double model_cost_;
    DynamicList<LaplaceStats> dynamic_list_;
    void GetDynamicListIndex(const Tuple& tuple, std::vector<size_t>* index);
    static std::vector<size_t> GetPredictorList(const Schema& schema, 
                                                const std::vector<size_t>& predictor_list);

  public:
    TableLaplace(const Schema& schema, const std::vector<size_t>& predictor_list,
                  size_t target_var, bool predict_int, double err);
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
