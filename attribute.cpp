#include "attribute.h"

#include "base.h"

#include <map>
#include <memory>
#include <string>

namespace db_compress {

namespace {

std::map<int, std::unique_ptr<AttrValueCreator> > repository;
std::map<int, int> base;

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
AttrValue* EnumAttrValueCreator::GetAttrValue(const std::string& str) {
    return new EnumAttrValue(std::stoi(str));
}

AttrValue* EnumAttrValueCreator::GetAttrValue(size_t val) {
    return new EnumAttrValue(val);
}

void EnumAttrValueCreator::ReadAttrValue(const AttrValue& attr, size_t *val) {
    *val = static_cast<const EnumAttrValue*>(&attr)->Value();
}

// Utility
AttrValueCreator* GetAttrValueCreator(int attr_type) {
    return repository[attr_type].get();
}

void RegisterAttrValueCreator(int attr_type, AttrValueCreator* creator, int base_type) {
    std::unique_ptr<AttrValueCreator> ptr(creator);
    repository[attr_type] = std::move(ptr);
    base[attr_type] = base_type;
}

int GetBaseType(int attr_type) {
    return base[attr_type];
}

void TupleCopy(Tuple* target, const Tuple& source, const Schema& schema) {
    for (size_t i = 0; i < schema.attr_type.size(); ++i ) {
        AttrValue* attr_value;
        AttrValue* src_attr = source.attr[i].get();
        switch (GetBaseType(schema.attr_type[i])) {
          case BASE_TYPE_INTEGER:
            {
                int val = static_cast<IntegerAttrValue*>(src_attr)->Value();
                attr_value = new IntegerAttrValue(val);
            }
            break;
          case BASE_TYPE_DOUBLE:
            {
                double val = static_cast<DoubleAttrValue*>(src_attr)->Value();
                attr_value = new DoubleAttrValue(val);
            }
            break;
          case BASE_TYPE_STRING:
            {
                std::string str = static_cast<StringAttrValue*>(src_attr)->Value();
                attr_value = new StringAttrValue(str);
            }
            break;
          case BASE_TYPE_ENUM:
            {
                size_t val = static_cast<EnumAttrValue*>(src_attr)->Value();
                attr_value = new EnumAttrValue(val);
            }
            break;
        }
        target->attr[i].reset(attr_value);
    }
}

} // namespace db_compress
