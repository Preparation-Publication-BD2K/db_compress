#include "base.h"
#include "utility.h"

#include <vector>

namespace db_compress {

void AdjustProbSegs(std::vector<double>* prob, const std::vector<double>& min_sep) {
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

}  // namespace db_compress
