#include "model.h"

#include "base.h"

#include <vector>
#include <map>
#include <memory>

namespace db_compress {

namespace {
    std::map<int, std::vector<std::unique_ptr<ModelCreator> > > model_rep;
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

Model::Model(const std::vector<size_t>& predictors, size_t target_var) :
    predictor_list_(predictors),
    target_var_(target_var) {}

Decoder* Model::GetDecoder(const Tuple& tuple, const ProbInterval& PIt,
                           const UnitProbInterval& PIb) {
    decoder_.reset(new Decoder(GetProbTree(tuple), PIt, PIb));
    return decoder_.get();
}

void Model::GetProbInterval(const Tuple& tuple, std::vector<ProbInterval>* prob_intervals,
                            std::unique_ptr<AttrValue>* result_attr) {
    ProbTree* prob_tree = GetProbTree(tuple);
    while (prob_tree->HasNextBranch()) {
        prob_tree->GenerateNextBranch();
        int branch = prob_tree->GetNextBranch(tuple.attr[target_var_]);
        if (prob_intervals != NULL)
            prob_intervals->push_back(prob_tree->GetProbInterval(branch));
        prob_tree->ChooseNextBranch(branch);
    }
    if (result_attr != NULL) {
        result_attr->reset(prob_tree->GetResultAttr());
    }
}

Decoder::Decoder(ProbTree* prob_tree, const ProbInterval& PIt, const UnitProbInterval& PIb) :
    prob_tree_(prob_tree),
    PIt_(PIt),
    PIb_(PIb) {
    if (!NextBranch()) {
        l_ = r_ = mid_ = 0;
    }
}

void Decoder::NextBoundary() {
    if (l_ == r_) {
        ProbInterval sub = prob_tree_->GetProbInterval(l_);
        PIt_ = GetPIProduct(PIt_, sub, &bytes_);
    } else {
        mid_ = (l_ + r_) / 2;
        boundary_ = GetPIRatioPoint(PIt_, prob_tree_->GetProbSegs()[mid_]);
    }
}

bool Decoder::NextBranch() {
    if (prob_tree_->HasNextBranch()) {
        prob_tree_->GenerateNextBranch();
        l_ = 0;
        r_ = prob_tree_->GetProbSegs().size();
        mid_ = (l_ + r_) / 2;
        if (l_ != r_)
            boundary_ = GetPIRatioPoint(PIt_, prob_tree_->GetProbSegs()[mid_]);
        return true;
    }
    return false;
}

}  // namespace db_compress
