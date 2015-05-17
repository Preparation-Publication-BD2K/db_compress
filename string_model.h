#ifndef STRING_MODEL_H
#define STRING_MODEL_H

#include "model.h"
#include "base.h"

#include <vector>

namespace db_compress {

class StringModel : public Model {
  private:
    std::vector<size_t> predictor_list_;
    size_t target_var_;
    std::vector<double> char_prob_;
    std::vector<double> length_prob_;
  public:
    StringModel(size_t target_var);
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
