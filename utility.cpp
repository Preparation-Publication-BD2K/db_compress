#include "utility.h"

#include "base.h"

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
