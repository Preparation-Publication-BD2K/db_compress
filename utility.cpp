#include "utility.h"

#include "base.h"

#include <iostream>
#include <cmath>
#include <vector>

namespace db_compress {

void AdjustProbIntervals(std::vector<double>* prob, const std::vector<double>& min_sep) {
    // First we deal with the trivial case
    if (prob->size() == 0) return;

    std::vector<double> true_prob;
    true_prob.push_back(prob->at(0));
    for (size_t i = 1; i < prob->size(); ++i )
        true_prob.push_back(prob->at(i) - prob->at(i - 1));
    true_prob.push_back(1 - prob->at(prob->size() - 1));
    
    std::vector<double> converted_prob(true_prob.size(), 0);
    std::vector<bool> converted(true_prob.size(), false);
    double used_prob = 0, remain_prob = 1;
    while (1) {
        bool found = false;
        for (size_t i = 0; i < true_prob.size(); ++i )
        if (true_prob[i] / remain_prob * (1 - used_prob) < min_sep[i] && !converted[i]) {
            used_prob += min_sep[i];
            remain_prob -= true_prob[i];
            converted_prob[i] = min_sep[i];
            converted[i] = true;
            found = true;
        }
        if (!found) break;
    }
    // Some error occured
    if (used_prob > 1) {
        std::cerr << "Error while adjusting prob intervals\n";
        return;
    }
    
    for (size_t i = 0; i < true_prob.size(); ++i )
    if (!converted[i]) {
        converted_prob[i] = true_prob[i] / remain_prob * (1 - used_prob);
    }

    prob->at(0) = converted_prob[0];
    for (size_t i = 1; i + 1 < true_prob.size(); ++ i)
        prob->at(i) = prob->at(i - 1) + converted_prob[i];
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
    // The special case where cnt is all zero vector, we also simply return all zero vector
    if (fabs(sum) < 0.01)
        return;
    for (size_t i = 0; i < prob->size(); i++ )
        prob->at(i) /= sum;
    
    AdjustProbIntervals(prob, min_sep);

    // Quantization
    for (size_t i = 0; i < prob->size(); i++ )
        prob->at(i) = round(prob->at(i) * quant_const) / quant_const;
}

ProbInterval ReducePIProduct(const ProbInterval& left, const ProbInterval& right,
                             std::vector<unsigned char>* emit_bytes) {
    double span = left.r - left.l;
    double l = left.l + span * right.l;
    double r = left.l + span * right.r;

    if (emit_bytes != NULL) {
        while (1) {
            int bracket = (int)floor(l * 256);
            if (l * 256 >= bracket && r * 256 <= bracket + 1) {
                emit_bytes->push_back((unsigned char)bracket);
                l = l * 256 - bracket;
                r = r * 256 - bracket;
            } else {
                break;
            }
        }
    }

    return ProbInterval(l, r);
}

ProbInterval ReducePIProduct(const std::vector<ProbInterval>& vec,
                             std::vector<unsigned char>* emit_bytes) {
    if (vec.size() == 0) return ProbInterval(0, 1);
    
    ProbInterval ret = vec[0];
    for (size_t i = 1; i < vec.size(); ++i ) {
        ret = ReducePIProduct(ret, vec[i], emit_bytes);
    }
    return ret;
}

void ReducePI(ProbInterval* PIt, ProbInterval* PIb) {
    while (PIt->l >= 0.5 || PIt->r <= 0.5) {
        if (PIt->l >= 0.5) {
            PIt->l = PIt->l * 2 - 1;
            PIt->r = PIt->r * 2 - 1;
            PIb->l = PIb->l * 2 - 1;
            PIb->r = PIb->r * 2 - 1;
        } else {
            PIt->l *= 2;
            PIt->r *= 2;
            PIb->l *= 2;
            PIb->r *= 2;
        }
    }
}

double GetMidValueFromExponential(double lambda, double lvalue, double rvalue) {
    double range = (rvalue == -1 ? -1 : rvalue - lvalue);
    double prob = (range == -1 ? 0.5 : exp(-range / lambda) / 2 + 0.5);
    return - log(prob) * lambda + lvalue;
}

void GetPartitionPointFromExponential(double lambda, double lvalue, double rvalue,
                                      double bin_size, double *prob, double *value) {
    double value_mid = GetMidValueFromExponential(lambda, lvalue, rvalue);
    int bin_index = (int)ceil((value_mid - lvalue) / bin_size); 
    value_mid = bin_index * bin_size + lvalue;
    *prob = 1 - exp(-(value_mid - lvalue) / lambda);
    *value = value_mid;
    // For finite interval, we need to normalize the probability
    if (rvalue != -1)
        *prob /= 1 - exp(-(rvalue - lvalue) / lambda);
}

void GetProbIntervalFromExponential(double lambda, double val, double err, bool target_int,
                                    bool reversed, double *result_val, 
                                    std::vector<ProbInterval> *ret) {
    double value_l = 0, value_r = -1;
    double bin_size = err * 2 + (target_int ? 1 : 0);
    while (1) {
        double value_mid, prob;
        GetPartitionPointFromExponential(lambda, value_l, value_r, 
                                         bin_size, &prob, &value_mid);

        if (val < value_mid - (target_int ? 0.5 : 0)) {
            value_r = value_mid;
            if (ret != NULL) {
                if (!reversed) 
                    ret->push_back(ProbInterval(0, prob));
                else
                    ret->push_back(ProbInterval(1 - prob, 1));
            }
        } else {
            value_l = value_mid;
            if (ret != NULL) {
                if (!reversed)
                    ret->push_back(ProbInterval(prob, 1));
                else
                    ret->push_back(ProbInterval(0, 1 - prob));
            }
        }
        // Note that value_l and value_r should always be multiples of bin_size
        if (value_r != -1 && value_r - value_l < bin_size * 1.5) break;
    }
    double value_ret = (value_l + value_r) / 2;
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

double ConvertSinglePrecision(unsigned char bytes[4]) {
    int exponent = (bytes[0] & 0x7f) * 2 + ((bytes[1] >> 7) & 1) - 127;
    int val = ((bytes[1] & 0x7f) << 16) + (bytes[2] << 8) + bytes[3];
    double ret = (val + (1 << 23)) * pow(2, exponent - 23);
    if ((bytes[0] & 0x80) > 0)
        return -ret;
    else
        return ret;
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

void GetBitStringFromProbInterval(BitString *str, const ProbInterval& prob) {
    str->Clear();
    double l = prob.l, r = prob.r;
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
