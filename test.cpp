#include <stdio.h>

#include "fuzzy_index.hpp"

int main(int argc, char *argv[])
{
    FuzzyIndex fi("test");
    auto s = string("This @is an _ artist!!! (モーニング娘。) The Gold It’s in The… ");
    auto ret = fi.encode_string(s);
    auto text = ret[0];
    auto remainder = ret[1];       
    cout << "'" << text << "'" << endl;
    cout << "'" << remainder << "'" << endl;

    s = string(" @_!!! \t ");
    cout << fi.encode_string_for_stupid_artists(s) << endl;
    return 0;
}
