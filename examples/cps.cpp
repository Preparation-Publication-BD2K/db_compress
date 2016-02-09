#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>

using namespace std;

const char infile[5][15] = {"jun00pub.cps", "jul00pub.dat", "aug00pub.cps", "sep00pub.cps", "oct00pub.cps"};
const char outfile[] = "cps.dat";
const char configfile[] = "cps.config";

int field_pos[] = {1, 16, 18, 22, 24, 27, 29, 31, 33, 35, 37, 39, 41, 
43, 45, 47, 57, 59, 61, 63, 65, 67, 69, 71, 75, 77, 
79, 81, 83, 85, 87, 89, 91, 93, 95, 97, 101, 104, 105, 106, 107, 108, 109, 114, 116, 118, 120, 122, 
124, 125, 127, 129, 131, 133, 135, 137, 139, 141, 143, 145, 147, 151, 153, 155, 157, 159, 161, 163, 
166, 169, 172, 174, 176, 178, 180, 182, 184, 186, 188, 190, 192, 194, 196, 198, 200, 202, 204, 206, 
208, 210, 212, 214, 216, 218, 220, 222, 224, 227, 229, 231, 233, 235, 237, 239, 241, 243, 245, 247, 
250, 252, 257, 259, 261, 263, 265, 267, 269, 271, 273, 275, 277, 279, 281, 283, 286, 288, 290, 292, 
294, 296, 298, 300, 302, 304, 306, 308, 310, 312, 314, 316, 318, 320, 322, 324, 326, 328, 330, 332, 
334, 336, 338, 340, 342, 345, 347, 349, 351, 353, 355, 357, 359, 361, 363, 365, 367, 369, 371, 373, 
375, 377, 379, 381, 383, 385, 387, 389, 391, 393, 395, 397, 399, 401, 403, 405, 407, 410, 412, 414, 
416, 418, 420, 422, 424, 426, 428, 430, 432, 434, 436, 439, 442, 444, 446, 449, 452, 454, 456, 458, 
460, 462, 464, 466, 468, 470, 472, 474, 476, 478, 480, 482, 484, 486, 488, 490, 492, 494, 496, 498, 
500, 502, 504, 506, 508, 512, 516, 520, 524, 525, 527, 535, 536, 538, 540, 548, 556, 559, 561, 563,
565, 567, 569, 571, 573, 575, 577, 579, 581, 583, 593, 603, 613, 623, 633, 635, 637, 639, 641, 643, 
645, 647, 649, 651, 653, 655, 657, 659, 661, 663, 665, 667, 669, 671, 673, 675, 677, 679, 681, 683, 
685, 687, 689, 691, 693, 695, 697, 699, 701, 703, 705, 707, 709, 711, 713, 715, 717, 719, 721, 723, 
725, 727, 729, 731, 733, 735, 737, 739, 741, 743, 745, 747, 749, 751, 753, 755, 757, 759, 761, 763, 
765, 767, 769, 771, 773, 775, 777, 779, 781, 783, 785, 787, 789, 791, 793, 795, 797, 799, 801, 803, 
805, 807, 809, 811, 813, 815, 820, 822, 824, 826, 828, 830, 832, 834, 836, 838, 840, 842, 844, 846, 
856};

map<string, int> dict[sizeof(field_pos) / sizeof(int)];

void ParseLine(const string& line, vector<string>* vec) {
    for (int i = 0; i < sizeof(field_pos) / sizeof(int) - 1; ++i) {
        string item = line.substr(field_pos[i] - 1, field_pos[i + 1] - field_pos[i]);
        vec->push_back(item);
    }
}

int GetIndex(map<string, int>* dict, const string& value) {
    int index;
    if (dict->count(value) == 0) {
        index = dict->size();
        (*dict)[value] = index;
    }
    else index = (*dict)[value];
    return index;
}

int main() {
    ofstream fout(outfile);
    ofstream fconfig(configfile);
    for (int d = 0; d < 5; ++d) {
        ifstream fin(infile[d]);

        string line;
        while (getline(fin, line)) {
            vector<string> vec;
            ParseLine(line, &vec);
            for (int i = 0; i < vec.size(); ++i) {
                if (vec[i].length() <= 2 || field_pos[i] == 71) {
                    fout << GetIndex(&dict[i], vec[i]);
                } else {
                    fout << vec[i];
                }
                if (i == vec.size() - 1)
                    fout << "\n";
                else
                    fout << ",";
            }
        }
    }
    for (int i = 0; i < sizeof(field_pos) / sizeof(int) - 1; ++i) {
        if (field_pos[i + 1] - field_pos[i] <= 2 || field_pos[i] == 71)
            fconfig << "ENUM " << dict[i].size() << " 0\n";
        else if (field_pos[i + 1] - field_pos[i] > 10)
            fconfig << "STRING\n";
        else
            fconfig << "INTEGER 0\n";
    }
}
