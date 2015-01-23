#ifndef MODEL_IMPL_H
#define MODEL_IMPL_H

#include "model.h"
#include "base.h"

#include <vector>

namespace db_compress {

template<class T>
class DynamicList {
  private:
    std::vector<std::vector<size_t>> dynamic_index_;
    std::vector<T> dynamic_list_;
  public:
    DynamicList() : dynamic_index_(1) {}
    T& operator[](const std::vector<size_t>& index);
    T operator[](const std::vector<size_t>& index) const;
    size_t size() const { return dynamic_list_.size(); }
    T& operator[](int index) { return dynamic_list_[index]; }
};

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

class GuassianProbDist : public ProbDist {
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
    double err_;
    double model_cost_;
    // Each vector consists of k-1 probability segment boundary
    DynamicList<std::vector<double>> dynamic_list_;
    void GetDynamicListIndex(const Tuple& tuple, std::vector<size_t>* index);
   
  public:
    TableCategorical(const Schema& schema, const std::vector<size_t>& predictor_list, 
                    size_t target_var, double err);
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
    std::vector<size_t> predictor_list_;
    std::vector<size_t> predictor_range_;
    size_t target_var_;
    double err_;
    bool target_int_;
    double model_cost_;
    DynamicList<GuassStats> dynamic_list_;
    void GetDynamicListIndex(const Tuple& tuple, std::vector<size_t>* index);

  public:
    TableGuassian(const Schema& schema, const std::vector<size_t>& predictor_list,
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

// Not actually a model, it simply reads in bytes and emits string.
class StringModel : public Model {
  private:
    std::vector<size_t> predictor_list_;
    size_t target_var_;
  public:
    StringModel(size_t target_var);
    ProbDist* GetProbDist(const Tuple& tuple, const ProbInterval& prob_interval);
    ProbInterval GetProbInterval(const Tuple& tuple, const ProbInterval& prob_interval, 
                                 std::vector<unsigned char>* emit_bytes);
    const std::vector<size_t>& GetPredictorList() const;
    size_t GetTargetVar() const;
    int GetModelCost() const;

    int GetModelDescriptionLength() const;
    void WriteModel(ByteWriter* byte_writer, size_t block_index) const;
};

template<class T>
T& DynamicList<T>::operator[](const std::vector<size_t>& index) {
    size_t pos = 0;
    for (size_t i = 0; i < index.size(); i++ ) {
        if (dynamic_index_[pos].size() <= index[i]) {
            // We use -1 as indicator of "empty" slot
            dynamic_index_[pos].resize(index[i] + 1, -1);
        }
        if (dynamic_index_[pos][index[i]] == -1) {
            size_t new_pos;
            if (i == index.size() - 1) {
                new_pos = dynamic_list_.size();
                dynamic_list_.push_back(T());
            }
            else {
                new_pos = dynamic_index_.size();
                dynamic_index_.push_back(std::vector<size_t>());
            }
            dynamic_index_[pos][index[i]] = new_pos;
            pos = new_pos;
        }
        else
            pos = dynamic_index_[pos][index[i]];
    }
    return dynamic_list_[pos];
}

template<class T>
T DynamicList<T>::operator[](const std::vector<size_t>& index) const {
    size_t pos = 0;
    for (size_t i = 0; i < index.size(); i++ ) {
        if (dynamic_index_[pos].size() <= index[i])
            return T();
        if (dynamic_index_[pos][index[i]] == -1) 
            return T();
        else
            pos = dynamic_index_[pos][index[i]];
    }
    return dynamic_list_[pos];
}

} // namespace db_compress

#endif
