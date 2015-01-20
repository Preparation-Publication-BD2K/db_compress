#include "attribute.h"
#include "base.h"

#include <vector>
#include <memory>

namespace db_compress {

namespace {

std::vector<std::unique_ptr<AttrValueCreator> > repository;
std::vector<int> base;

} // anonymous namespace

// IntegerAttrValueCreator
AttrValue* IntegerAttrValueCreator::GetAttrValue(const std::string& str) {
    return new IntegerAttrValue(std::stoi(str));
}

AttrValue* IntegerAttrValueCreator::GetAttrValue(int val) {
    return new IntegerAttrValue(val);
}

void IntegerAttrValueCreator::ReadAttrValue(const AttrValue& attr, int* val) {
    *val = static_cast<const IntegerAttrValue*>(&attr)->Value();
}

// DoubleAttrValueCreator
AttrValue* DoubleAttrValueCreator::GetAttrValue(const std::string& str) {
    return new DoubleAttrValue(std::stod(str));
}

AttrValue* DoubleAttrValueCreator::GetAttrValue(double val) {
    return new DoubleAttrValue(val);
}

void DoubleAttrValueCreator::ReadAttrValue(const AttrValue& attr, double* val) {
    *val = static_cast<const DoubleAttrValue*>(&attr)->Value();
}

// StringAttrValueCreator
AttrValue* StringAttrValueCreator::GetAttrValue(const std::string& str) {
    return new StringAttrValue(str);
}

void StringAttrValueCreator::ReadAttrValue(const AttrValue& attr, std::string *val) {
    *val = static_cast<const StringAttrValue*>(&attr)->Value();
}

// EnumAttrValueCreator
AttrValue* EnumAttrValueCreator::GetAttrValue(size_t val) {
    return new EnumAttrValue(val);
}

void EnumAttrValueCreator::ReadAttrValue(const AttrValue& attr, size_t *val) {
    *val = static_cast<const EnumAttrValue*>(&attr)->Value();
}

// Utility
AttrValueCreator* GetAttrValueCreator(size_t attr_type) {
    return repository[attr_type].get();
}

void RegisterAttrValueCreator(size_t attr_type, AttrValueCreator* creator, int base_type) {
    if (attr_type >= repository.size()) {
        repository.resize(attr_type + 1);
        base.resize(attr_type + 1);
    }
    std::unique_ptr<AttrValueCreator> ptr(creator);
    repository[attr_type] = std::move(ptr);
    base[attr_type] = base_type;
}

int GetBaseType(size_t attr_type) {
    return base[attr_type];
}

} // namespace db_compress
