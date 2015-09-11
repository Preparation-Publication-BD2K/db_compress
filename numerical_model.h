#ifndef NUMERICAL_MODEL_H
#define NUMERICAL_MODEL_H

#include "model.h"
#include "base.h"
#include "utility.h"

#include <vector>

namespace db_compress {

class IntegerAttrValue: public AttrValue {
  private:
    int value_;
  public:
    IntegerAttrValue(int val) : value_(val) {}
    inline int Value() const { return value_; }
};

class DoubleAttrValue: public AttrValue {
  private:
    double value_;
  public:
    DoubleAttrValue(double val) : value_(val) {}
    inline double Value() const { return value_; }
};

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
    ProbInterval PIt_;
    ProbInterval PIb_;
    double mean_, dev_, err_;
    bool target_int_, reversed_;
    double l_, r_, mid_;
    double boundary_, bin_size_;

    void Advance();
  public:
    LaplaceProbDist(const LaplaceStats& stats, const ProbInterval& PIt, 
                    const ProbInterval& PIb, double err, bool target_int);
    bool IsEnd() const;
    void FeedBit(bool bit);
    ProbInterval GetPIt() const;
    ProbInterval GetPIb() const;
    AttrValue* GetResult() const;
};

class TableLaplace : public Model {
  private:
    std::vector<size_t> predictor_list_;
    std::vector<size_t> predictor_range_;
    std::vector<const AttrInterpreter*> predictor_interpreter_;
    size_t target_var_;
    double err_;
    bool target_int_;
    double model_cost_;
    DynamicList<LaplaceStats> dynamic_list_;
    std::unique_ptr<LaplaceProbDist> prob_dist_;

    void GetDynamicListIndex(const Tuple& tuple, std::vector<size_t>* index);

  public:
    TableLaplace(const Schema& schema, const std::vector<size_t>& predictor_list,
                  size_t target_var, double err, bool target_int);
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
    static Model* ReadModel(ByteReader* byte_reader, 
                            const Schema& schema, size_t index, bool target_int);
};

class TableLaplaceRealCreator : public ModelCreator {
  public:
    Model* ReadModel(ByteReader* byte_reader, const Schema& schema, size_t index);
    Model* CreateModel(const Schema& schema, const std::vector<size_t>& predictor_list,
                       size_t target_var, double err);
};

class TableLaplaceIntCreator : public ModelCreator {
  public:
    Model* ReadModel(ByteReader* byte_reader, const Schema& schema, size_t index);
    Model* CreateModel(const Schema& schema, const std::vector<size_t>& predictor_list,
                       size_t target_var, double err);
};

} // namespace db_compress

#endif
