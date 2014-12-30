#include "attribute.h"
#include "base.h"

#include <vector>
#include <memory>

namespace db_compress {

namespace {

std::vector<std::unique_ptr<AttrValueCreator> > repository;
std::vector<int> base;

} // anonymous namespace

AttrValue* IntegerAttrValueCreator::GetAttrValue(const std::string& str) {
    return new IntegerAttrValue(std::stoi(str));
}

AttrValue* IntegerAttrValueCreator::GetAttrValue(int val) {
    return new IntegerAttrValue(val);
}

AttrValueCreator* GetAttrValueCreator(int attr_type) {
    return repository[attr_type].get();
}

void RegisterAttrValueCreator(int attr_type, AttrValueCreator* creator, int base_type) {
    if (attr_type >= repository.size()) {
        repository.resize(attr_type + 1);
        base.resize(attr_type + 1);
    }
    std::unique_ptr<AttrValueCreator> ptr(creator);
    repository[attr_type] = std::move(ptr);
    base[attr_type] = base_type;
}

int GetBaseType(int attr_type) {
    return base[attr_type];
}

} // namespace db_compress
