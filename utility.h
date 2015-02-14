#ifndef UTILITY_H
#define UTILITY_H

#include "base.h"

#include <iostream>
#include <vector>

namespace db_compress {

/*
 * Dynamic List behaves like multi-dimensional array, except that the number of dimensions
 * can vary across different instances. The number of dimensions need to be specified in
 * the constructor.
 */
template<class T>
class DynamicList {
  private:
    std::vector<std::vector<size_t>> dynamic_index_;
    std::vector<T> dynamic_list_;
    size_t index_len_;
  public:
    DynamicList(size_t index_len);
    T& operator[](const std::vector<size_t>& index);
    T operator[](const std::vector<size_t>& index) const;
    size_t size() const { return dynamic_list_.size(); }
    T& operator[](int index) { return dynamic_list_[index]; }
};

template<class T>
DynamicList<T>::DynamicList(size_t index_len) :
    dynamic_index_(1), 
    index_len_(index_len) {}

template<class T>
T& DynamicList<T>::operator[](const std::vector<size_t>& index) {
    if (index.size() != index_len_) {
        std::cerr << "Inconsistent Dynamic List Index Length\n";
    }
    size_t pos = 0;
    for (size_t i = 0; i < index.size(); ++i ) {
        if (dynamic_index_[pos].size() <= index[i]) {
            // We use -1 as indicator of "empty" slot
            dynamic_index_[pos].resize(index[i] + 1, -1);
        }
        if (dynamic_index_[pos][index[i]] == -1) {
            size_t new_pos;
            if (i == index.size() - 1) {
                new_pos = dynamic_list_.size();
                dynamic_list_.push_back(T());
            }
            else {
                new_pos = dynamic_index_.size();
                dynamic_index_.push_back(std::vector<size_t>());
            }
            dynamic_index_[pos][index[i]] = new_pos;
            pos = new_pos;
        }
        else
            pos = dynamic_index_[pos][index[i]];
    }
    if (index_len_ == 0 && dynamic_list_.size() == 0)
        dynamic_list_.push_back(T());
    return dynamic_list_[pos];
}

template<class T>
T DynamicList<T>::operator[](const std::vector<size_t>& index) const {
    if (index.size() != index_len_) {
        std::cerr << "Inconsistent Dynamic List Index Length\n";
    }
    size_t pos = 0;
    for (size_t i = 0; i < index.size(); ++i ) {
        if (dynamic_index_[pos].size() <= index[i])
            return T();
        if (dynamic_index_[pos][index[i]] == -1) 
            return T();
        else
            pos = dynamic_index_[pos][index[i]];
    }
    if (index_len_ == 0 && dynamic_list_.size() == 0)
        return T();
    return dynamic_list_[pos];
}

/*
 * Apply minimal adjustments to the k-1 probability interval boundaries to make sure that
 * all the k probability intervals satisfies the minimal separation constraints.
 *
 * For example, when prob = {0.2, 0.4, 0.6}, which represents probability intervals [0, 0.2],
 * [0.2, 0.4], [0.4, 0.6], [0.6, 1], and min_sep = {0.3, 0, 0.1, 0}, which means that after 
 * adjustment, the four intervals will have length at least 0.3, 0, 0.1, 0, respectively.
 *
 * The function will try to minimize cross entropy during adjustments.
 */
void AdjustProbIntervals(std::vector<double>* prob, const std::vector<double>& min_sep);

/*
 * The quantization procedure transforms raw count of each individual bins to probability
 * interval boundaries, such that all probility boundaries are integral multiples of 
 * (1 / quant_const)
 */
void Quantization(std::vector<double>* prob, const std::vector<double>& cnt, double quant_const);

/*
 * Get new probability interval from old probability interval and current probability 
 * subinterval, emit bytes when possible, the emitted bytes are directly concatenated 
 * to the end of emit_bytes (i.e., do not initialize emit_bytes).
 */
void GetProbSubinterval(double old_l, double old_r, double sub_l, double sub_r, 
                        double *new_l, double *new_r, std::vector<unsigned char>* emit_bytes);

/*
 * Get mid value from exponential value interval, note that we use "-1" to represent infinity
 * for rvalue.
 */
double GetMidValueFromExponential(double lambda, double lvalue, double rvalue);

/*
 * Get the ProbInterval of given value from given exponential probability distribution,
 * with error threshold given.
 */
void GetProbIntervalFromExponential(double lambda, double val, double err, bool target_int,
                                    double old_l, double old_r, bool reversed, 
                                    double *result_val, double *l, double *r, 
                                    std::vector<unsigned char>* emit_bytes);

/*
 * Convert val to a single precision float number.
 */
void QuantizationToFloat32Bit(double* val);

/*
 * Convert single precision float number to raw bytes.
 */
void ConvertSinglePrecision(double val, unsigned char bytes[4]);

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
void GetBitStringFromProbInterval(BitString *str, double l, double r);

} // namespace db_compress

#endif
