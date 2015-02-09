#include "utility.h"

#include "base.h"

#include <cmath>
#include <vector>

namespace db_compress {

void AdjustProbIntervals(std::vector<double>* prob, const std::vector<double>& min_sep) {
    if (prob->size() > 0 && prob->at(0) < min_sep[0]) {
        prob->at(0) = min_sep[0];
    }
    for (size_t i = 1; i < prob->size(); i++ ) {
        if (prob->at(i) <= prob->at(i - 1) + min_sep[i])
            prob->at(i) = prob->at(i - 1) + min_sep[i];
    }
    if (prob->size() > 0) {
        if (prob->at(prob->size() - 1) > 1 - min_sep[prob->size()])
            prob->at(prob->size() - 1) = 1 - min_sep[prob->size()];
    }
    for (int i = prob->size() - 1; i >= 1; i-- ) {
        if (prob->at(i) <= prob->at(i - 1) + min_sep[i])
            prob->at(i - 1) = prob->at(i) - min_sep[i];
    }
}

void Quantization(std::vector<double>* prob, const std::vector<double>& cnt, 
                  double quant_const) {
    // Mark zero entries
    std::vector<double> min_sep;
    for (size_t i = 0; i < cnt.size(); i++ )
    if (cnt[i] == 0)
        min_sep.push_back(0);
    else
        min_sep.push_back(1 / quant_const);

    // Calculate Probability Vector
    double sum = cnt[0];
    prob->clear();
    for (size_t i = 1; i < cnt.size(); i++ ) {
        prob->push_back(sum);
        sum += cnt[i];
    }
    for (size_t i = 0; i < prob->size(); i++ )
        prob->at(i) /= sum;
    
    AdjustProbIntervals(prob, min_sep);

    // Quantization
    for (size_t i = 0; i < prob->size(); i++ )
        prob->at(i) = round(prob->at(i) * quant_const) / quant_const;
}

void GetProbSubinterval(double old_l, double old_r, double sub_l, double sub_r,
                        double *new_l, double *new_r, std::vector<unsigned char>* emit_bytes) {
    double span = old_r - old_l;
    double l = old_l + span * sub_l;
    double r = old_l + span * sub_r;

    if (emit_bytes != NULL) {
        while (1) {
            int bracket = (int)floor(l * 256);
            if (l * 256 > bracket && r * 256 < bracket + 1) {
                emit_bytes->push_back((unsigned char)bracket);
                l = l * 256 - bracket;
                r = r * 256 - bracket;
            } else {
                break;
            }
        }
    }

    *new_l = l;
    *new_r = r;
}

double GetMidValueFromExponential(double lambda, double lvalue, double rvalue) {
    double prob_l, prob_r;
    prob_l = 1 - exp(-lvalue / lambda);
    prob_r = (rvalue == -1 ? 1 : 1 - exp(-rvalue / lambda));
    double mid = (prob_l + prob_r) / 2;
    return - log(1 - mid) * lambda;
}

void GetProbIntervalFromExponential(double lambda, double val, double err, bool target_int,
                                    double *result_val, double *l, double *r) {
    double l_ = 0, r_ = 1;
    double value_l = 0, value_r = -1;
    while (1) {
        double value_mid = GetMidValueFromExponential(lambda, value_l, value_r);
        if (val < value_mid) {
            value_r = (target_int ? floor(value_mid) : value_mid);
            r_ = (l_ + r_) / 2;
        } else {
            value_l = (target_int ? ceil(value_mid) : value_mid);
            l_ = (l_ + r_) / 2;
        }
        if (value_r != -1 && value_r - value_l <= 2 * err) break;
    }
    double value_ret = (value_l + value_r) / 2;
    *l = l_; *r = r_; 
    *result_val = (target_int ? floor(value_ret) : value_ret);
}

void QuantizationToFloat32Bit(double* val) {
    *val = (float)(*val);
}

void ConvertSinglePrecision(double val, unsigned char bytes[4]) {
    bytes[0] = bytes[1] = bytes[2] = bytes[3] = 0;
    if (val == 0) return;
    if (val < 0) {
        val = -val;
        bytes[0] |= 128;
    }
    int exponent = 127;
    while (val < 1) {
        val *= 2;
        -- exponent;
    }
    while (val >= 2) {
        val /= 2;
        ++ exponent;
    }
    bytes[0] |= (((unsigned char)exponent >> 1) & 127);
    bytes[1] |= (((unsigned char)exponent & 1) << 7);
    unsigned int fraction = round(val * (1 << 23));
    bytes[1] |= ((fraction >> 16) & 0x7f);
    bytes[2] = ((fraction >> 8) & 0xff);
    bytes[3] = (fraction & 0xff);
}

void StrCat(BitString* str, unsigned char byte) {
    int index = str->length / 32;
    int offset = str->length & 31;
    if (offset == 0)
        str->bits.push_back(0);
    if (offset <= 24) {
        str->bits[index] |= (byte << (24 - offset));
        str->length += 8;
    } else {
        StrCat(str, byte, 8);
    }
}

void StrCat(BitString* str, unsigned bits, int len) {
    int index = str->length / 32;
    int offset = str->length & 31;
    bits &= (1 << len) - 1;
    if (offset == 0)
        str->bits.push_back(0);
    if (offset + len <= 32) {
        str->bits[index] |= (bits << (32 - len - offset));
        str->length += len;
    } else {
        int extra_bits = len + offset - 32;
        str->bits[index] |= (bits >> extra_bits);
        str->length += 32 - offset;
        StrCat(str, bits, extra_bits);
    }
}

void StrCat(BitString* str, const BitString& cat) {
    for (size_t i = 0; i < cat.length / 32; ++i )
        StrCat(str, cat.bits[i], 32);
    if ((cat.length & 31) > 0) {
        int len = cat.length & 31;
        StrCat(str, cat.bits[cat.length / 32] >> (32 - len), len);
    }
}

void GetBitStringFromProbInterval(BitString *str, double l, double r) {
    str->Clear();
    while (l > 0 || r < 1) {
        int offset = str->length & 31;
        if (offset == 0)
             str->bits.push_back(0);
        unsigned& last = str->bits[str->length / 32];

        if (0.5 - l > r - 0.5) {
            r *= 2;
            l *= 2;
        } else {
            last |= (1 << (31 - offset));
            l = l * 2 - 1;
            r = r * 2 - 1;
        }
        str->length ++;
    }

}

}  // namespace db_compress
