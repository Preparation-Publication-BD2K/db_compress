#ifndef STRING_MODEL_H
#define STRING_MODEL_H

#include "model.h"
#include "base.h"
#include "categorical_model.h"

#include <vector>

namespace db_compress {

class StringAttrValue: public AttrValue {
  private:
    std::string value_;
  public:
    StringAttrValue(const std::string& val) : value_(val) {}
    inline const std::string& Value() const { return value_; }
};

class StringProbTree : public ProbTree {
  private:
    const std::vector<Prob>& char_prob_, len_prob_;
    bool is_end_;
    int len_;
    std::string result_;
  public:
    StringProbTree(const std::vector<Prob>& char_prob, const std::vector<Prob>& len_prob);
    bool HasNextBranch() const;
    void GenerateNextBranch();
    int GetNextBranch(const AttrValue* attr) const;
    void ChooseNextBranch(int branch);
    AttrValue* GetResultAttr() const;
};

class StringModel : public Model {
  private:
    std::vector<Prob> char_prob_, length_prob_;
    std::vector<int> char_count_, length_count_;

    std::unique_ptr<StringProbTree> prob_tree_;
  public:
    StringModel(size_t target_var);
    ProbTree* GetProbTree(const Tuple& tuple);
    int GetModelCost() const;

    void FeedTuple(const Tuple& tuple);
    void EndOfData();

    int GetModelDescriptionLength() const;
    void WriteModel(ByteWriter* byte_writer, size_t block_index) const;
    static Model* ReadModel(ByteReader* byte_reader, size_t index);
};

class StringModelCreator : public ModelCreator {
  public:
    Model* ReadModel(ByteReader* byte_reader, const Schema& schema, size_t index);
    Model* CreateModel(const Schema& schema, const std::vector<size_t>& predictor,
                       size_t index, double err);
};

} // namespace db_compress

#endif
