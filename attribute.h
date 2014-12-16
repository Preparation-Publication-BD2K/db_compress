#ifndef ATTRIBUTE_H
#define ATTRIBUTE_H

#include "compression.h"
#include <string>

/*
 * each specialized AttrValue class instantiate the virtual AttrValue class.
 */
class IntegerAttrValue: public AttrValue {
  private:
    int value_;
  public:
    inline int Value();
};

class DoubleAttrValue: public AttrValue {
  private:
    double value_;
  public:
    inline double Value();
};

class StringAttrValue: public AttrValue {
  private:
    string value_;
  public:
    inline const string& Value();
};

class EnumAttrValue: public AttrValue {
  private:
    int value_;
  public:
    inline int Value();
};

/*
 * AttrValueCreator is used to create ClassattrValue, we separate AttrValueCreator with
 * AttrValue to provide class extensibility. In general, each instance of AttrValueCreator
 * maps to each schema type defined in data catalog, but each type of specialized AttrValue maps
 * to each physical type in implementation. Generally, multiple instances AttrValueCreator
 * maps to the same type of ClassAttrValue (but they may have different constructors associtaed
 * with them).
 */
virtual class AttrValueCreator {
  public:
    virtual *AttrValue GetAttrValue(const stirng& str) = 0;
}

class IntegerAttrValueCreator: public AttrValueCreator {
  public:
    *AttrValue GetAttrValue(const string& str);
};

class DoubleAttrValueCreator: public AttrValueCreator {
  public:
    *AttrValue GetAttrValue(const string& str);
};

class StringAttrValueCreator: public AttrValueCreator {
  public:
    *AttrValue GetAttrValue(const string& str);
};

class EnumAttrValueCreator: public AttrValueCreator {
  public:
    *AttrValue GetAttrValue(const stirng& str);
};

/*
 * This function registers the AttrValueCreator to the registry, which can be used later to 
 * create attributes. Each AttrValueCreator is associated with one attr_type number. This
 * function takes the ownership of AttrValueCreator object.
 */
void RegisterAttrValueCreator(int attr_type, AttrValueCreator* creator);

#endif
