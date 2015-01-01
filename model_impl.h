#ifndef MODEL_IMPL_H
#define MODEL_IMPL_H

#include "model.h"
#include "base.h"

#include <vector>

namespace db_compress {

template<class T>
class DynamicList {
  private:
    std::vector<std::vector<int>> dynamic_index_;
    std::vector<T> dynamic_list_;
  public:
    T& GetValue(const std::vector<int>& index);
    int GetNumOfElements() { return dynamic_list_.size(); }
    T& GetValue(int index) { return dynamic_list_[index]; }
};

class TableCategorical : public Model {
  private:
    std::vector<int> predictor_list_;
    std::vector<int> predictor_range_;
    int target_var_;
    int target_range_;
    double err_;
    DynamicList<std::vector<int>> dynamic_list_;
   
  public:
    TableCategorical(const Schema& schema, const std::vector<int>& predictor_list, 
                    int target_var, double err);
    ProbDist* GetProbDist(const Tuple& tuple, const ProbInterval& prob_interval);
    ProbInterval GetProbInterval(const Tuple& tuple, const ProbInterval& prob_interval,
                                 std::vector<char>* emit_bytes);
    const std::vector<int>& GetPredictorList() const;
    int GetTargetVar() const;
    int GetModelCost() const;
    void FeedTuple(const Tuple& tuple);
    void EndOfData();
};

struct GuassStats {
    int count;
    double sum;
    double sqr_sum;
    double mean;
    double std;
    GuassStats() : count(0), sum(0), sqr_sum(0) {}
};

class TableGuassian : public Model {
  private:
    std::vector<int> predictor_list_;
    std::vector<int> predictor_range_;
    int target_var_;
    double err_;
    bool target_int_;
    DynamicList<GuassStats> dynamic_list_;
  public:
    TableGuassian(const Schema& schema, const std::vector<int>& predictor_list,
                  int target_var, bool predict_int, double err);
    ProbDist* GetProbDist(const Tuple& tuple, const ProbInterval& prob_interval);
    ProbInterval GetProbInterval(const Tuple& tuple, const ProbInterval& prob_interval, 
                                 std::vector<char>* emit_bytes);
    const std::vector<int>& GetPredictorList() const;
    int GetTargetVar() const;
    int GetModelCost() const;
    void FeedTuple(const Tuple& tuple);
    void EndOfData();
};

// Not actually a model, it simply reads in bytes and emits string.
class StringModel : public Model {
  private:
    std::vector<int> predictor_list_;
    int target_var_;
  public:
    StringModel(int target_var);
    ProbDist* GetProbDist(const Tuple& tuple, const ProbInterval& prob_interval);
    ProbInterval GetProbInterval(const Tuple& tuple, const ProbInterval& prob_interval, 
                                 std::vector<char>* emit_bytes);
    const std::vector<int>& GetPredictorList() const;
    int GetTargetVar() const;
    int GetModelCost() const;
};

template<class T>
T& DynamicList<T>::GetValue(const std::vector<int>& index) {
    int pos = 0;
    for (int i = 0; i < index.size(); i++ ) {
        if (dynamic_index_[pos].size() <= index[i]) {
            dynamic_index_[pos].resize(index[i] + 1);
        }
        if (dynamic_index_[pos][index[i]] == 0) {
            int new_pos;
            if (i == index.size() - 1) {
                new_pos = dynamic_list_.size();
                dynamic_list_.push_back(T());
            }
            else {
                new_pos = dynamic_index_.size();
                dynamic_index_.push_back(std::vector<int>());
            }
            dynamic_index_[pos][index[i]] = new_pos;
            pos = new_pos;
        }
        else
            pos = dynamic_index_[pos][index[i]];
    }
    return dynamic_list_[pos];
}

} // namespace db_compress

#endif
