#ifndef GAUSSIAN_MODEL_H
#define GAUSSIAN_MODEL_H

#include "model.h"
#include "base.h"
#include "utility.h"

#include <vector>

namespace db_compress {

struct GaussianStats {
    int count;
    double sum;
    double sqr_sum;
    double mean;
    double std;
    GaussianStats() : count(0), sum(0), sqr_sum(0) {}
};

class GaussianProbDist : public ProbDist {
  private:
  public:
    bool End();
    void FeedByte(char byte);
    int GetUnusedBits();
    ProbInterval GetRemainProbInterval();
    AttrValue* GetResult();
    void Reset();
};

class TableGaussian : public Model {
  private:
    std::vector<size_t> predictor_list_;
    std::vector<size_t> predictor_range_;
    size_t target_var_;
    double err_;
    bool target_int_;
    double model_cost_;
    DynamicList<GaussianStats> dynamic_list_;
    void GetDynamicListIndex(const Tuple& tuple, std::vector<size_t>* index);
    static std::vector<size_t> GetPredictorList(const Schema& schema, 
                                                const std::vector<size_t>& predictor_list);

  public:
    TableGaussian(const Schema& schema, const std::vector<size_t>& predictor_list,
                  size_t target_var, bool predict_int, double err);
    ProbDist* GetProbDist(const Tuple& tuple, const ProbInterval& prob_interval);
    ProbInterval GetProbInterval(const Tuple& tuple, const ProbInterval& prob_interval, 
                                 std::vector<unsigned char>* emit_bytes);
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
