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
    IntegerAttrValue() {}
    IntegerAttrValue(int val) : value_(val) {}
    inline void Set(int val) { value_ = val; }
    inline int Value() const { return value_; }
};

class DoubleAttrValue: public AttrValue {
  private:
    double value_;
  public:
    DoubleAttrValue() {}
    DoubleAttrValue(double val) : value_(val) {}
    void Set(double val) { value_ = val; }
    inline double Value() const { return value_; }
};

struct LaplaceStats {
  private:
    void GetMedian();
  public:
    int count;
    double median;
    double sum_abs_dev;
    double mean_abs_dev;
    std::vector<double> values;
    LaplaceStats() : count(0), median(0), sum_abs_dev(0) {}
    void PushValue(double value);
    void End(double bin_size);
};

class LaplaceProbTree : public ProbTree {
  private:
    bool target_int_;
    double bin_size_;

    double mean_, dev_;
    int l_, r_, mid_;
    bool l_inf_, r_inf_;

    IntegerAttrValue int_attr_;
    DoubleAttrValue double_attr_;

    void SetLeft(int l) { l_ = l; l_inf_ = false; }
    void SetRight(int r) { r_ = r; r_inf_ = false; }
  public:
    LaplaceProbTree(double bin_size, bool target_int);
    void Init(const LaplaceStats& stats);
    bool HasNextBranch() const;
    void GenerateNextBranch();
    int GetNextBranch(const AttrValue* attr) const;
    void ChooseNextBranch(int branch);
    const AttrValue* GetResultAttr();
};

class TableLaplace : public Model {
  private:
    std::vector<const AttrInterpreter*> predictor_interpreter_;
    bool target_int_;
    double bin_size_;
    double model_cost_;
    DynamicList<LaplaceStats> dynamic_list_;
    LaplaceProbTree prob_tree_;

    void GetDynamicListIndex(const Tuple& tuple, std::vector<size_t>* index);

  public:
    TableLaplace(const Schema& schema, const std::vector<size_t>& predictor_list,
                  size_t target_var, double err, bool target_int);
    ProbTree* GetProbTree(const Tuple& tuple);
    int GetModelCost() const { return model_cost_; }
    void FeedTuple(const Tuple& tuple);
    void EndOfData();

    int GetModelDescriptionLength() const;
    void WriteModel(ByteWriter* byte_writer, size_t block_index) const;
    static Model* ReadModel(ByteReader* byte_reader, 
                            const Schema& schema, size_t index, bool target_int);
};

class TableLaplaceRealCreator : public ModelCreator {
  private:
    const size_t MAX_TABLE_SIZE = 10000;
  public:
    Model* ReadModel(ByteReader* byte_reader, const Schema& schema, size_t index);
    Model* CreateModel(const Schema& schema, const std::vector<size_t>& predictor_list,
                       size_t target_var, double err);
};

class TableLaplaceIntCreator : public ModelCreator {
  private:
    const size_t MAX_TABLE_SIZE = 10000;
  public:
    Model* ReadModel(ByteReader* byte_reader, const Schema& schema, size_t index);
    Model* CreateModel(const Schema& schema, const std::vector<size_t>& predictor_list,
                       size_t target_var, double err);
};

} // namespace db_compress

#endif
