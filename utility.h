#ifndef UTILITY_H
#define UTILITY_H

#include "base.h"
#include "model.h"

#include <vector>

namespace db_compress {

template<class T>
class DynamicList {
  private:
    std::vector<std::vector<size_t>> dynamic_index_;
    std::vector<T> dynamic_list_;
  public:
    DynamicList() : dynamic_index_(1) {}
    T& operator[](const std::vector<size_t>& index);
    T operator[](const std::vector<size_t>& index) const;
    size_t size() const { return dynamic_list_.size(); }
    T& operator[](int index) { return dynamic_list_[index]; }
};

template<class T>
T& DynamicList<T>::operator[](const std::vector<size_t>& index) {
    size_t pos = 0;
    for (size_t i = 0; i < index.size(); i++ ) {
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
    return dynamic_list_[pos];
}

template<class T>
T DynamicList<T>::operator[](const std::vector<size_t>& index) const {
    size_t pos = 0;
    for (size_t i = 0; i < index.size(); i++ ) {
        if (dynamic_index_[pos].size() <= index[i])
            return T();
        if (dynamic_index_[pos][index[i]] == -1) 
            return T();
        else
            pos = dynamic_index_[pos][index[i]];
    }
    return dynamic_list_[pos];
}

void AdjustProbSegs(std::vector<double>* prob, const std::vector<double>& min_sep);

} // namespace db_compress

#endif
