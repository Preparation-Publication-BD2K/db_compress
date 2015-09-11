#ifndef CATEGORICAL_MODEL_H
#define CATEGORICAL_MODEL_H

#include "model.h"
#include "base.h"
#include "utility.h"

#include <vector>

namespace db_compress {

class EnumAttrValue: public AttrValue {
  private:
    size_t value_;
  public:
    EnumAttrValue(size_t val) : value_(val) {}
    inline size_t Value() const { return value_; }
};

class CategoricalProbDist : public ProbDist {
  private:
    const std::vector<double>& prob_segs_;
    size_t l_, r_, mid_;
    ProbInterval PIt_, PIb_;
    double boundary_;

    void Advance();
  public:
    CategoricalProbDist(const std::vector<double>& prob_segs, const ProbInterval& PIt,
                        const ProbInterval& PIb);
    bool IsEnd() const;
    void FeedBit(bool bit);
    ProbInterval GetPIt() const;
    ProbInterval GetPIb() const;
    AttrValue* GetResult() const;
};

class TableCategorical : public Model {
  private:
    std::vector<size_t> predictor_list_;
    std::vector<size_t> predictor_range_;
    std::vector<const AttrInterpreter*> predictor_interpreter_;
    size_t target_var_;
    size_t target_range_;
    size_t cell_size_;
    double err_;
    double model_cost_;
    std::unique_ptr<CategoricalProbDist> prob_dist_;

    // Each vector consists of k-1 probability segment boundary
    DynamicList<std::vector<double>> dynamic_list_;
    void GetDynamicListIndex(const Tuple& tuple, std::vector<size_t>* index);
   
  public:
    TableCategorical(const Schema& schema, const std::vector<size_t>& predictor_list, 
                    size_t target_var, double err);
    // Model owns the ProbDist object
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
    static Model* ReadModel(ByteReader* byte_reader, const Schema& schema, size_t index);
};

class TableCategoricalCreator : public ModelCreator {
  public:
    Model* ReadModel(ByteReader* byte_reader, const Schema& schema, size_t index);
    Model* CreateModel(const Schema& schema, const std::vector<size_t>& predictor, 
                       size_t index, double err);
};

} // namespace db_compress

#endif
