#ifndef MODEL_H
#define MODEL_H

#include "base.h"
#include "data_io.h"
#include "utility.h"

#include <vector>
#include <set>
#include <map>
#include <memory>

namespace db_compress {

/*
 * The ProbTree class defines the interface of branching, which can be used in
 * encoding & decoding. This interface simplifies the process of defining models
 * for new attributes
 */
class ProbTree {
  protected:
    std::vector<Prob> prob_segs_;
  public:
    virtual ~ProbTree() = 0;
    // Return false if reached leave node
    virtual bool HasNextBranch() const = 0;
    virtual void GenerateNextBranch() = 0;
    virtual int GetNextBranch(const AttrValue* attr) const = 0;
    virtual void ChooseNextBranch(int branch) = 0;
    // Caller takes ownership of AttrValue
    virtual AttrValue* GetResultAttr() const = 0;

    const std::vector<Prob>& GetProbSegs() const { return prob_segs_; }
    ProbInterval GetProbInterval(int branch) const;
};

inline ProbTree::~ProbTree() {}

inline ProbInterval ProbTree::GetProbInterval(int branch) const {
    Prob l = GetZeroProb(), r = GetOneProb();
    if (branch > 0)
        l = prob_segs_[branch - 1];
    if (branch < (int)prob_segs_.size())
        r = prob_segs_[branch];
    return ProbInterval(l, r);
}

/*
 * ProbDist Class is initialized with two ProbIntervals and some Probability
 * Distribution, it reads in bit string, reaches certain unit bin in the distribution
 * and emits the result bin and two reduced ProbIntervals.
 */
class Decoder {
  private:
    ProbTree* prob_tree_;
    size_t l_, r_, mid_;
    ProbInterval PIt_;
    UnitProbInterval PIb_;
    std::vector<unsigned char> bytes_;
    Prob boundary_;

    void Advance();
    void NextBoundary();
    bool NextBranch();
  public:
    Decoder(ProbTree* prob_tree, const ProbInterval& PIt, const UnitProbInterval& PIb);
    bool IsEnd() const { return !prob_tree_->HasNextBranch(); }
    void FeedBit(bool bit);
    ProbInterval GetPIt() const { return PIt_; }
    UnitProbInterval GetPIb() const { return PIb_; }
    // Caller takes ownership of AttrValue.
    AttrValue* GetResult() const { return prob_tree_->GetResultAttr(); }
};

inline void Decoder::FeedBit(bool bit) {
    if (bit)
        PIb_.GoRight();
    else
        PIb_.GoLeft();
    Advance();
}

inline void Decoder::Advance() {
    while (1) {
        while (r_ > l_) {
            if (boundary_ >= PIb_.Right())
                r_ = mid_;
            else if (boundary_ <= PIb_.Left())
                l_ = mid_ + 1;
            else return;
            NextBoundary();
        }

        if (bytes_.size() > 0) {
            if (PIb_.exp >= (int)(bytes_.size() * 8)) {
                ReducePI(&PIb_, bytes_);
                bytes_.clear();
            } else return;
        } else if (PIb_.Left() >= PIt_.l && PIb_.Right() <= PIt_.r) {
            prob_tree_->ChooseNextBranch(l_);
            if (!NextBranch()) return;
        } else return;
    }
}

/*
 * The Model class represents the local conditional probability distribution. The
 * Model object can be used to generate ProbDist object which can be used to infer
 * the result attribute value based on bitstring (decompressing). It can also be used to
 * create ProbInterval object which can be used for compressing.
 */
class Model {
  private:
    unsigned char creator_index_;
    std::unique_ptr<Decoder> decoder_;
  protected:
    std::vector<size_t> predictor_list_;
    size_t target_var_;
  public:
    Model(const std::vector<size_t>& predictors, size_t target_var);
    virtual ~Model() = 0;
    // The Model class owns the ProbTree object
    virtual ProbTree* GetProbTree(const Tuple& tuple) = 0;
    // Get an estimation of model cost, which is used in model selection process.
    virtual int GetModelCost() const = 0;

    // Learning
    virtual void FeedTuple(const Tuple& tuple) { }
    virtual void EndOfData() { }

    // Model Description
    virtual int GetModelDescriptionLength() const = 0;
    virtual void WriteModel(ByteWriter* byte_writer, size_t block_index) const = 0;

    void SetCreatorIndex(unsigned char index) { creator_index_ = index; }
    unsigned char GetCreatorIndex() const { return creator_index_; }
    const std::vector<size_t>& GetPredictorList() const { return predictor_list_; }
    size_t GetTargetVar() const { return target_var_; }

    // The Model class owns the Decoder object.
    Decoder* GetDecoder(const Tuple& tuple, const ProbInterval& PIt, 
                        const UnitProbInterval& PIb);

    // The results are appended to the end of prob_intervals vector and resultAttr 
    // will be set as the modified result AttrValue
    void GetProbInterval(const Tuple& tuple, std::vector<ProbInterval>* prob_intervals,
                         std::unique_ptr<AttrValue>* result_attr);
};

inline Model::~Model() {}

/*
 * The ModelCreator class is used to create Model object either from compressed file or
 * from scratch, the ModelCreator classes must be registered to be applied during
 * training process.
 */
class ModelCreator {
  public:
    virtual ~ModelCreator() = 0;
    // Caller takes ownership
    virtual Model* ReadModel(ByteReader* byte_reader, const Schema& schema, size_t index) = 0;
    // Caller takes ownership, return NULL if predictors don't match
    virtual Model* CreateModel(const Schema& schema, const std::vector<size_t>& predictor,
                               size_t index, double err) = 0;
};

inline ModelCreator::~ModelCreator() {}

class AttrInterpreter {
  public:
    virtual ~AttrInterpreter();
    virtual bool EnumInterpretable() const { return false; }
    virtual int EnumCap() const { return 0; }
    virtual int EnumInterpret(const AttrValue* attr) const { return 0; }

    virtual double NumericInterpretable() const { return false; }
    virtual double NumericInterpret(const AttrValue* attr) const { return 0; }
};

inline AttrInterpreter::~AttrInterpreter() {}

/*
 * This function registers the ModelCreator, which can be used to create models.
 * Multiple ModelCreators can be associated with the same attr_type number. 
 * This function takes the ownership of ModelCreator object.
 */
void RegisterAttrModel(int attr_type, ModelCreator* creator);
const std::vector<ModelCreator*>& GetAttrModel(int attr_type);

/*
 * This function registers the AttrInterpreter, which can be used to interpret
 * attributes so that they can be used as predictors for other attributes.
 * In our implementation, each attribute type could have only one interpreter.
 * This function takes the ownership of ModelCreator object.
 */
void RegisterAttrInterpreter(int attr_type, AttrInterpreter* interpreter);
const AttrInterpreter* GetAttrInterpreter(int attr_type);

} // namespace db_compress

#endif
