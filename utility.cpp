#include "utility.h"

#include "base.h"

#include <iostream>
#include <cmath>
#include <vector>

namespace db_compress {

/*
 * The quantization follows two steps:
 * 1. Mark all categories consists of less than 1/2^base portion of total count
 * 2. Distribute the remaining probability proportionally
 */
void Quantization(std::vector<Prob>* prob, const std::vector<int>& cnt, int base) {
    std::vector<int> prob_cnt(cnt.size(), 0);

    int sum = 0;
    for (size_t i = 0; i < cnt.size(); ++i)
        sum += cnt[i];

    long long total = (1 << base);
    // We have to make sure that every prob has num < 2^base
    if (cnt[cnt.size() - 1] == 0)
        -- total;
    while (1) {
        bool found = false;
        for (size_t i = 0; i < cnt.size(); ++i)
        if (cnt[i] > 0 && cnt[i] * total < sum && prob_cnt[i] == 0) {
            prob_cnt[i] = 1;
            -- total;
            sum -= cnt[i];
        }
        if (!found) break;
    }

    for (size_t i = 0; i < cnt.size(); ++i)
    if (cnt[i] > 0 && prob_cnt[i] == 0) {
        int share = cnt[i] * total / sum;
        prob_cnt[i] = share;
        total -= share;
        sum -= cnt[i];
    }

    sum = 0;
    prob->clear();
    for (size_t i = 0; i < prob_cnt.size() - 1; ++i) {
        sum += prob_cnt[i];
        prob->push_back(GetProb(sum, base));
    }
}

ProbInterval GetPIProduct(const ProbInterval& left, const ProbInterval& right,
                             std::vector<unsigned char>* emit_bytes) {
    Prob range(left.r - left.l);
    ProbInterval product(left.l + range * right.l, left.l + range * right.r);

    if (emit_bytes != NULL) {
        while (1) {
            while (1) {
                int bracket = CastInt(product.l, 8);
                if (product.r <= GetProb(bracket + 1, 8)) {
                    product.l = product.l - GetProb(bracket, 8);
                    product.r = product.r - GetProb(bracket, 8);
                    product.l.exp -= 8;
                    product.r.exp -= 8;
                    emit_bytes->push_back((unsigned char)bracket);
                } else break;
            }
            product.l.Reduce();
            product.r.Reduce();
            if (product.l.exp <= 16 && product.r.exp <= 16) break; 
            Prob new_left = product.l, new_right;
            if (product.l > GetProb(CastInt(product.l, 16), 16))
                new_left = GetProb(CastInt(product.l, 16) + 1, 16);
            new_right = GetProb(CastInt(product.r, 16), 16);

            if (new_left != new_right) {
                product.l = new_left;
                product.r = new_right;
            } else {
                if (new_left - product.l < product.r - new_right)
                    product.l = new_left;
                else
                    product.r = new_right;
            }
        }
    }
    return product;
}

ProbInterval ReducePIProduct(const std::vector<ProbInterval>& vec,
                             std::vector<unsigned char>* emit_bytes) {
    if (vec.size() == 0) return ProbInterval(GetZeroProb(), GetOneProb());
    
    ProbInterval ret = vec[0];
    for (size_t i = 1; i < vec.size(); ++i ) {
        ret = GetPIProduct(ret, vec[i], emit_bytes);
    }
    return ret;
}

Prob GetPIRatioPoint(const ProbInterval& PI, const Prob& ratio) {
    return PI.l + (PI.r - PI.l) * ratio;
}

void ReducePI(UnitProbInterval* PI, const std::vector<unsigned char>& bytes) {
    for (size_t i = 0; i < bytes.size(); ++i) {
        long long byte = bytes[i];
        // PI->l = (PI->l - byte/256) * 256, PI->r = (PI->r - byte/256) * 256
        PI->exp -= 8;
        PI->num -= byte << PI->exp;
    }
}

void QuantizationToFloat32Bit(double* val) {
    unsigned char bytes[4];
    ConvertSinglePrecision(*val, bytes);
    *val = ConvertSinglePrecision(bytes);
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
    if (exponent == -127 && val == 0) return 0;
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
    UnitProbInterval PI = GetWholeProbInterval();
    while (PI.Left() < prob.l || PI.Right() > prob.r) {
        int offset = str->length & 31;
        if (offset == 0)
             str->bits.push_back(0);
        unsigned& last = str->bits[str->length / 32];

        if (PI.Mid() - prob.l > prob.r - PI.Mid()) {
            PI.GoLeft();
        } else {
            last |= (1 << (31 - offset));
            PI.GoRight();
        }
        str->length ++;
    }

}

}  // namespace db_compress
