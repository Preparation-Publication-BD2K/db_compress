// This header defines many utility functions

#ifndef UTILITY_H
#define UTILITY_H

#include "base.h"

#include <iostream>
#include <vector>
#include <cmath>

namespace db_compress {

/*
 * Dynamic List behaves like multi-dimensional array, except that the number of dimensions
 * can vary across different instances. The number of dimensions need to be specified in
 * the constructor.
 */
template<class T>
class DynamicList {
  private:
    std::vector<T> dynamic_list_;
    std::vector<size_t> index_cap_;
  public:
    DynamicList(const std::vector<size_t>& index_cap);
    T& operator[](const std::vector<size_t>& index);
    const T& operator[](const std::vector<size_t>& index) const;
    T& operator[](int index) { return dynamic_list_[index]; }
    const T& operator[] (int index) const { return dynamic_list_[index]; }
    size_t size() const { return dynamic_list_.size(); }
};

template<class T>
DynamicList<T>::DynamicList(const std::vector<size_t>& index_cap) :
    index_cap_(index_cap) {
    int size = 1;
    for (size_t i = 0; i < index_cap.size(); ++i)
        size *= index_cap[i];
    dynamic_list_.resize(size);
    dynamic_list_.shrink_to_fit();
}

template<class T>
T& DynamicList<T>::operator[](const std::vector<size_t>& index) {
    if (index.size() != index_cap_.size()) {
        std::cerr << "Inconsistent Dynamic List Index Length\n";
    }
    size_t pos = 0;
    for (size_t i = 0; i < index.size(); ++i )
        pos = pos * index_cap_[i] + index[i];
    return dynamic_list_[pos];
}

template<class T>
const T& DynamicList<T>::operator[](const std::vector<size_t>& index) const {
    if (index.size() != index_cap_.size()) {
        std::cerr << "Inconsistent Dynamic List Index Length\n";
    }
    size_t pos = 0;
    for (size_t i = 0; i < index.size(); ++i )
        pos = pos * index_cap_[i] + index[i];
    return dynamic_list_[pos];
}

/*
 * The quantization procedure transforms raw count of each individual bins to probability
 * interval boundaries, such that all probability boundary will have base 16. This function
 * will try to minimize cross entropy during quantization
 */
void Quantization(std::vector<Prob>* prob, const std::vector<int>& cnt, int base);

/*
 * The following functions are used to generate Prob structure, or cast Prob
 * structure to primitive types
 */
// Prob = count / (2^base)
inline Prob GetProb(int count, int base) { return Prob(count, base); }
// Round to base 16
inline Prob GetProb(double value) { return Prob((int)round(value * (1 << 16)), 16); }
inline Prob GetZeroProb() { return Prob(0, 0); }
inline Prob GetOneProb() { return Prob(1, 0); }
inline UnitProbInterval GetWholeProbInterval() { return UnitProbInterval(0, 0); }
// Trunc to nearest value within that base
inline int CastInt(const Prob& prob, int base) {
    if (prob.exp <= base)
        return prob.num << (base - prob.exp);
    else
        return prob.num >> (prob.exp - base);
}
inline double CastDouble(const Prob& prob) {
    return (double)prob.num / ((long long)1 << prob.exp);
}

/*
 * The following are operators on Prob structure
 */
inline bool operator<(const Prob& left, const Prob& right) {
    return (left.num << (40 - left.exp)) < (right.num << (40 - right.exp));
}
inline bool operator<=(const Prob& left, const Prob& right) {
    return (left.num << (40 - left.exp)) <= (right.num << (40 - right.exp));
}
inline bool operator>=(const Prob& left, const Prob& right) {
    return (left.num << (40 - left.exp)) >= (right.num << (40 - right.exp));
}
inline bool operator>(const Prob& left, const Prob& right) {
    return (left.num << (40 - left.exp)) > (right.num << (40 - right.exp));
}
inline bool operator==(const Prob& left, const Prob& right) {
    return (left.num << (40 - left.exp)) == (right.num << (40 - right.exp));
}
inline bool operator!=(const Prob& left, const Prob& right) {
    return (left.num << (40 - left.exp)) != (right.num << (40 - right.exp));
}
inline Prob operator+(const Prob& left, const Prob& right) {
    if (left.exp < right.exp)
        return Prob((left.num << (right.exp - left.exp)) + right.num, right.exp);
    else
        return Prob(left.num + (right.num << (left.exp - right.exp)), left.exp);
}
inline Prob operator-(const Prob& left, const Prob& right) {
    if (left.exp < right.exp)
        return Prob((left.num << (right.exp - left.exp)) - right.num, right.exp);
    else
        return Prob(left.num - (right.num << (left.exp - right.exp)), left.exp);
}
inline Prob operator*(const Prob& left, const Prob& right) {
    return Prob(left.num * right.num, left.exp + right.exp);
}

// Get the length of given ProbInterval
inline Prob GetLen(const ProbInterval& PI) { return PI.r - PI.l; }
/*
 * Compute the product of probability intervals and emits bytes when possible, 
 * the emitted bytes are directly concatenated to the end of emit_bytes (i.e., do
 * not initialize emit_bytes)
 */
ProbInterval GetPIProduct(const ProbInterval& left, const ProbInterval& right,
                             std::vector<unsigned char>* emit_bytes);

/*
 * Reduce a vector of probability intervals to their product
 */
ProbInterval ReducePIProduct(const std::vector<ProbInterval>& vec,
                             std::vector<unsigned char>* emit_bytes);

/*
 * Compute the ratio point of the ProbInterval
 */
Prob GetPIRatioPoint(const ProbInterval& PI, const Prob& ratio);

/*
 * Reduce ProbInterval according given bytes
 */
void ReducePI(UnitProbInterval* PIt, const std::vector<unsigned char>& bytes);

/*
 * Get value of cumulative distribution function of exponential distribution
 */
inline double GetCDFExponential(double lambda, double x) {
    return 1 - exp(-x / lambda);
}

/*
 * Convert val to a single precision float number.
 */
void QuantizationToFloat32Bit(double* val);

/*
 * Convert single precision float number to raw bytes.
 */
void ConvertSinglePrecision(double val, unsigned char bytes[4]);

/*
 * Convert single precision float number from raw bytes.
 */
double ConvertSinglePrecision(unsigned char bytes[4]);

/*
 * Extract one byte from 32-bit unsigned int
 */
inline unsigned char GetByte(unsigned bits, int start_pos) {
    return (unsigned char)(((bits << start_pos) >> 24) & 0xff);
}

/*
 * BitStirng stucture stores bit strings in the format of:
 *   bits[0],   bits[1],    ..., bits[n]
 *   1-32 bits, 33-64 bits, ..., n*32+1-n*32+k bits
 *
 *   bits[n] = **************000000000000000
 *             |-- k bits---|---32-k bits--|
 *   length = n*32+k
 */
struct BitString {
    std::vector<unsigned> bits;
    unsigned length;
    inline void Clear() {
        bits.clear();
        length = 0;
    }
};

// Concatenate one byte
void StrCat(BitString* str, unsigned char byte);
// Only takes the least significant bits
void StrCat(BitString* str, unsigned bits, int len);
// Concatenate two BitStrings
void StrCat(BitString* str, const BitString& cat);
// PadBitString is used to pad zeros in the BitString as suffixes 
inline void PadBitString(BitString* bit_string, size_t target_length) {
    bit_string->length = target_length;
    while (bit_string->bits.size() < (target_length + 31) / 32)
        bit_string->bits.push_back(0);
}
// Note that prefix_length need to be less than 32.
inline unsigned ComputePrefix(const BitString& bit_string, int prefix_length) {
    int shift_bit = 32 - prefix_length;
    return (bit_string.bits[0] >> shift_bit) & ((1 << prefix_length) - 1);
}
// Find the minimal BitString which has the corresponding probability interval lies within [l, r]
void GetBitStringFromProbInterval(BitString *str, const ProbInterval& prob);

} // namespace db_compress

#endif
