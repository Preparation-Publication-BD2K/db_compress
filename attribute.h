#ifndef ATTRIBUTE_H
#define ATTRIBUTE_H

#include "base.h"
#include <string>

namespace db_compress {

const int BASE_TYPE_INTEGER = 0;
const int BASE_TYPE_DOUBLE = 1;
const int BASE_TYPE_STRING = 2;
const int BASE_TYPE_ENUM = 3;
/*
 * each specialized AttrValue class instantiate the virtual AttrValue class.
 */
class IntegerAttrValue: public AttrValue {
  private:
    int value_;
  public:
    IntegerAttrValue(int val) : value_(val) {}
    inline int Value() const { return value_; }
};

class DoubleAttrValue: public AttrValue {
  private:
    double value_;
  public:
    DoubleAttrValue(double val) : value_(val) {}
    inline double Value() const { return value_; }
};

class StringAttrValue: public AttrValue {
  private:
    std::string value_;
  public:
    StringAttrValue(const std::string& val) : value_(val) {}
    inline const std::string& Value() const { return value_; }
};

class EnumAttrValue: public AttrValue {
  private:
    int value_;
  public:
    EnumAttrValue(int val) : value_(val) {}
    inline int Value() const { return value_; }
};

/*
 * AttrValueCreator is used to create ClassAttrValue, we separate AttrValueCreator with
 * AttrValue to provide class extensibility. In general, each instance of AttrValueCreator
 * maps to each schema type defined in data catalog, but each type of specialized AttrValue maps
 * to each physical type in implementation. Generally, multiple instances AttrValueCreator
 * maps to the same type of ClassAttrValue (but they may have different constructors associtaed
 * with them).
 */
class AttrValueCreator {
  public:
    virtual ~AttrValueCreator() = 0;
    virtual AttrValue* GetAttrValue(const std::string& str) {
        return NULL;
    }
    virtual AttrValue* GetAttrValue(int val) {
        return NULL;
    }
    virtual AttrValue* GetAttrValue(double val) {
        return NULL;
    }
};

inline AttrValueCreator::~AttrValueCreator() {}

class IntegerAttrValueCreator: public AttrValueCreator {
  public:
    AttrValue* GetAttrValue(const std::string& str);
    AttrValue* GetAttrValue(int val);
};

class DoubleAttrValueCreator: public AttrValueCreator {
  public:
    AttrValue* GetAttrValue(const std::string& str);
    AttrValue* GetAttrValue(double val);
};

class StringAttrValueCreator: public AttrValueCreator {
  public:
    AttrValue* GetAttrValue(const std::string& str);
};

class EnumAttrValueCreator: public AttrValueCreator {
  public:
    AttrValue* GetAttrValue(const std::string& str);
    AttrValue* GetAttrValue(int val);
};

/*
 * This function registers the AttrValueCreator, which can be used later to create attributes.
 * Each AttrValueCreator is associated with one attr_type number. This function takes the
 * ownership of AttrValueCreator object.
 */
void RegisterAttrValueCreator(int attr_type, AttrValueCreator* creator, int base_type);
AttrValueCreator* GetAttrValueCreator(int attr_type);

/*
 * Get the base type of given attribute type.
 */
int GetBaseType(int attr_type);

}  // namespace db_compress

#endif
