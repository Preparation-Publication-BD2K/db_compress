/*
 * The base header files that defines several basic structures
 */

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
 * Tuple structure contains fixed number of pointers to attributes, 
 * the attribute types can be determined by Schema class. Note that
 * Tuple structures do not own the attribute value objects.
 */
struct Tuple {
    Tuple(int cols) : attr(cols) {}
    std::vector<const AttrValue*> attr;
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
 * Structure for probability values. We don't use float-point value here to
 * avoid precision overflow.
 */
struct Prob {
    long long num;
    char exp;
    Prob() : num(0), exp(0) {}
    Prob(long long num_, char exp_) : num(num_), exp(exp_) {}
    void Reduce() {
        if (num == 0) { exp = 0; return; }
        while ((num & 0xff) == 0) { num >>= 8; exp -= 8; }
        if ((num & 0xf) == 0) { num >>= 4; exp -= 4; }
        if ((num & 3) == 0) { num >>= 2; exp -= 2; }
        if ((num & 1) == 0) { num >>= 1; -- exp; }
    }
};

/*
 * Structure used to represent any probability interval between [0, 1].
 */
struct ProbInterval {
    Prob l, r;
    ProbInterval(const Prob& l_, const Prob& r_) : l(l_), r(r_) {}
};

/*
 * Structure used to represent unit probability interval (i.e., [n/2^k, (n+1)/2^k])
 */
struct UnitProbInterval {
    long long num;
    char exp;
    UnitProbInterval(int num_, char exp_) : num(num_), exp(exp_) {}
    Prob Left() { return Prob(num, exp); }
    Prob Right() { return Prob(num + 1, exp); }
    Prob Mid() { return Prob((num << 1) + 1, exp + 1); }
    ProbInterval GetProbInterval() { return ProbInterval(Left(),Right()); }
    void GoLeft() { num <<= 1; ++ exp; }
    void GoRight() { num <<= 1; ++ exp; ++ num; }
};

}
#endif
