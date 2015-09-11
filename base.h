#ifndef BASE_H
#define BASE_H

#include <memory>
#include <string>
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
    ~Tuple() {}
    std::vector<const AttrValue*> attr;
};

/*
 * The difference between ResultTuple and Tuple is that ResultTuple owns the attribute value
 * objects and is used in decoding process to hold the decoded attributes.
 */
struct ResultTuple {
    std::vector<std::unique_ptr<AttrValue>> attr;
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
 * Structure used to represent any probability interval between [0, 1].
 */
struct ProbInterval {
    double l, r;
    ProbInterval(double l_, double r_) : l(l_), r(r_) {}
};

}
#endif
