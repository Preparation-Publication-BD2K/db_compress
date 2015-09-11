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
    return interpreter_rep[attr_type].get();
}

}  // namespace db_compress
