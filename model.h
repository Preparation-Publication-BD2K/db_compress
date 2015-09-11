#ifndef MODEL_H
#define MODEL_H

#include "base.h"
#include "data_io.h"

#include <vector>
#include <set>
#include <map>
#include <memory>

namespace db_compress {

/*
 * ProbDist Class is initialized with two ProbIntervals and some Probability
 * Distribution, it reads in bit string, reaches certain unit bin in the distribution
 * and emits the result bin and two reduced ProbIntervals.
 */
class ProbDist {
  public:
    virtual ~ProbDist() = 0;
    virtual bool IsEnd() const = 0;
    virtual void FeedBit(bool bit) = 0;
    virtual ProbInterval GetPIt() const = 0;
    virtual ProbInterval GetPIb() const = 0;
    // Caller takes ownership of AttrValue.
    virtual AttrValue* GetResult() const = 0;
};

inline ProbDist::~ProbDist() {}

/*
 * The Model class represents the local conditional probability distribution. The
 * Model object can be used to generate ProbDist object which can be used to infer
 * the result attribute value based on bitstring (decompressing). It can also be used to
 * create ProbInterval object which can be used for compressing.
 */
class Model {
  public:
    virtual ~Model() = 0;
    // The Model class owns the ProbDist object.
    virtual ProbDist* GetProbDist(const Tuple& tuple, const ProbInterval& PIt,
                                  const ProbInterval& PIb) = 0;
    // If the model may modify the attributes during compression (lossy compression), then
    // resultAttr will be set as the modified result AttrValue, otherwise it may remain 
    // unaffected. The results are appended to the end of prob_intervals vector.
    virtual void GetProbInterval(const Tuple& tuple, std::vector<ProbInterval>* prob_intervals,
                                         std::unique_ptr<AttrValue>* result_attr) = 0;
    virtual const std::vector<size_t>& GetPredictorList() const = 0;
    virtual size_t GetTargetVar() const = 0;
    // Get an estimation of model cost, which is used in model selection process.
    virtual int GetModelCost() const = 0;

    // Learning
    virtual void FeedTuple(const Tuple& tuple) { }
    virtual void EndOfData() { }

    // Model Description
    virtual int GetModelDescriptionLength() const = 0;
    virtual void WriteModel(ByteWriter* byte_writer, size_t block_index) const = 0;
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
    virtual double NumericInterpret() const { return 0; }
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
