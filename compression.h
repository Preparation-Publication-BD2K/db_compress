#ifndef COMPRESSION_H
#define COMPRESSION_H

namespace db_compress {
/*
 * AttrValue is a virtual class for attribute values, all typed attribute
 * values will be subclass of this class.
 */ 
virtual class AttrValue {}

/*
 * Returns pointer to an AttrValue object based on attribute type and its
 * string representation.
 */ 
AttrValue* GetAttrValue(int attr_type, const string& str);

/*
 * Tuple structure contains num_attr_ of attributes, the attr_ array stores the pointers to
 * values of attributes, the attribute types can be determined by Schema class. Note that
 * Tuple structures do not own the attribute value objects.
 */
struct Tuple {
    const int num_attr_;
    Tuple(int cols) : num_attr_(cols) {}
    ~Tuple() {
        for (int i = 0; i < num_attr_; ++i)
            delete attr_[i];
    }
    (AttrValue*) attr_[num_attr_];
};

/*
 * Schema structure contains the attribute types information, which are used for type casting
 * when we need to interpret the values in each tuple.
 */
struct Schema {
    const int cols_;
    int attr_type_[cols_];
    Schema(int cols, int *attr_type);
};

}
#endif
