#ifndef STRING_MODEL_H
#define STRING_MODEL_H

#include "model.h"
#include "base.h"
#include "categorical_model.h"

#include <vector>

namespace db_compress {

class StringProbDist : public ProbDist {
  private:
    const std::vector<double>& char_prob_, len_prob_;
    ProbInterval PIt_, PIb_;
    bool is_end_;
    int len_;
    std::string result_;

    std::unique_ptr<CategoricalProbDist> len_prob_dist_, char_prob_dist_;    
  public:
    StringProbDist(const std::vector<double>& char_prob, const std::vector<double>& len_prob,
                   const ProbInterval& PIt, const ProbInterval& PIb);
    bool IsEnd() const;
    void FeedBit(bool bit);
    ProbInterval GetPIt() const;
    ProbInterval GetPIb() const;
    AttrValue* GetResult() const;
};

class StringModel : public Model {
  private:
    std::vector<size_t> predictor_list_;
    size_t target_var_;
    std::vector<double> char_prob_;
    std::vector<double> length_prob_;

    std::unique_ptr<StringProbDist> prob_dist_;
  public:
    StringModel(size_t target_var);
    ProbDist* GetProbDist(const Tuple& tuple, const ProbInterval& PIt, const ProbInterval& PIb);
    void GetProbInterval(const Tuple& tuple, std::vector<ProbInterval>* prob_intervals,
                                 std::unique_ptr<AttrValue>* result_attr);
    const std::vector<size_t>& GetPredictorList() const;
    size_t GetTargetVar() const;
    int GetModelCost() const;

    void FeedTuple(const Tuple& tuple);
    void EndOfData();

    int GetModelDescriptionLength() const;
    void WriteModel(ByteWriter* byte_writer, size_t block_index) const;
    static Model* ReadModel(ByteReader* byte_reader, size_t index);
};

} // namespace db_compress

#endif
