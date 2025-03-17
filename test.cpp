#include <stdio.h>

#include "fuzzy_index.hpp"

int main(int argc, char *argv[])
{
    FuzzyIndex fi("test");
    auto s = string("This @is an _ artist!!!");
    cout << fi.encode_string(s) << endl;

    s = string(" @_!!! \t ");
    cout << fi.encode_string_for_stupid_artists(s) << endl;
    return 0;
}
