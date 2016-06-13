#include "model.h"

#include "base.h"

#include <vector>
#include <map>
#include <memory>

namespace db_compress {

namespace {
    std::map<int, std::vector<std::unique_ptr<ModelCreator> > > model_rep;
    /* 
     * model_ptr is the replicate of model_rep, but without being unique_ptr
     * makes it possible to return it directly.
     */
    std::map<int, std::vector<ModelCreator*> > model_ptr;
    std::map<int, std::unique_ptr<AttrInterpreter> > interpreter_rep;
}  // anonymous namespace

void RegisterAttrModel(int attr_type, ModelCreator* creator) {
    std::unique_ptr<ModelCreator> ptr(creator);
    model_rep[attr_type].push_back(std::move(ptr));
    model_ptr[attr_type].push_back(creator);
}

const std::vector<ModelCreator*>& GetAttrModel(int attr_type) {
    return model_ptr[attr_type];
}

void RegisterAttrInterpreter(int attr_type, AttrInterpreter* interpreter) {
    interpreter_rep[attr_type].reset(interpreter);
}

const AttrInterpreter* GetAttrInterpreter(int attr_type) {
    if (interpreter_rep[attr_type] == nullptr)
        interpreter_rep[attr_type].reset(new AttrInterpreter());
    return interpreter_rep[attr_type].get();
}

// We have to explicitly define this because PIt and PIb do not have default constructor,
Decoder::Decoder() :
    PIt_(Prob(), Prob()),
    PIb_(0,0) {}

inline void Decoder::Init(SquID* squid, const ProbInterval& PIt,
                          const UnitProbInterval& PIb) {
    squid_ = squid;
    PIt_ = PIt;
    PIb_ = PIb;
    if (NextBranch()) 
        Advance();
}

void Decoder::NextBoundary() {
    if (l_ == r_) {
        ProbInterval sub = squid_->GetProbInterval(l_);
        PIt_ = GetPIProduct(PIt_, sub, &bytes_);
    } else {
        mid_ = (l_ + r_) / 2;
        boundary_ = GetPIRatioPoint(PIt_, squid_->GetProbSegs()[mid_]);
    }
}

bool Decoder::NextBranch() {
    if (squid_->HasNextBranch()) {
        squid_->GenerateNextBranch();
        l_ = 0;
        r_ = squid_->GetProbSegs().size();
        mid_ = (l_ + r_) / 2;
        if (l_ != r_)
            boundary_ = GetPIRatioPoint(PIt_, squid_->GetProbSegs()[mid_]);
        return true;
    }
    return false;
}

SquIDModel::SquIDModel(const std::vector<size_t>& predictors, size_t target_var) :
    predictor_list_(predictors),
    target_var_(target_var) {}

Decoder* SquIDModel::GetDecoder(const Tuple& tuple, const ProbInterval& PIt,
                           const UnitProbInterval& PIb) {
    decoder_.Init(GetSquID(tuple), PIt, PIb);
    return &decoder_;
}

void SquIDModel::GetProbInterval(const Tuple& tuple, std::vector<ProbInterval>* prob_intervals,
                            const AttrValue** result_attr) {
    SquID* squid = GetSquID(tuple);
    while (squid->HasNextBranch()) {
        squid->GenerateNextBranch();
        int branch = squid->GetNextBranch(tuple.attr[target_var_]);
        if (prob_intervals != NULL)
            prob_intervals->push_back(squid->GetProbInterval(branch));
        squid->ChooseNextBranch(branch);
    }
    if (result_attr != NULL) {
        *result_attr = squid->GetResultAttr();
    }
}

}  // namespace db_compress
