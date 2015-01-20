#ifndef BASE_H
#define BASE_H

#include <vector>

namespace db_compress {
/*
 * AttrValue is a virtual class for attribute values, all typed attribute
 * values will be subclass of this class.
 */ 
class AttrValue {
  public:
    virtual ~AttrValue() = 0;
};

inline AttrValue::~AttrValue() {}

/*
 * Tuple structure contains num_attr_ of attributes, the attr_ array stores the pointers to
 * values of attributes, the attribute types can be determined by Schema class. Note that
 * Tuple structures do not own the attribute value objects.
 */
struct Tuple {
    Tuple(int cols) {
        attr.resize(cols);
        attr.shrink_to_fit();
    }
    ~Tuple() {
        for (unsigned i = 0; i < attr.size(); ++i)
            delete attr[i];
    }
    Tuple(const Tuple& tuple) = delete;
    std::vector<AttrValue*> attr;
};

/*
 * Schema structure contains the attribute types information, which are used for type casting
 * when we need to interpret the values in each tuple.
 */
struct Schema {
    std::vector<int> attr_type;
    Schema() {}
    Schema(const std::vector<int>& attr_type_vec) : attr_type(attr_type_vec) {}
};

/* 
 * Returns an Schema object based on schema string representation,
 * caller takes ownership.
 */
//Schema* GetSchema(const vector<string>& schema);

}
#endif
