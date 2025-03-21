#include <stdio.h>

#include "fuzzy_index.hpp"

int main(int argc, char *argv[])
{
    FuzzyIndex fi("test");
   
    vector<IndexData> data;
    data.push_back(IndexData(1, "help"));
    data.push_back(IndexData(2, "data"));
    fi.build(data);
    printf("data indexed!\n");

#if 0
    auto s = string("Thiafadfadfadfas @is _ t!!! (モーニング娘。)fudfdf fsd It’s… ");
    auto ret = fi.encode_string(s);
    auto text = ret[0];
    auto remainder = ret[1];       
    cout << "'" << text << "'" << endl;
    cout << "'" << remainder << "'" << endl;

    s = string(" @_!!! \t  (*&(*&^*&^(*&)(%(&*%^*^%*&)(*+_(_(&(%)*(&+_&(%))))))))");
    ret = fi.encode_string_for_stupid_artists(s);
    text = ret[0];
    remainder = ret[1];       
    cout << "'" << text << "'" << endl;
    cout << "'" << remainder << "'" << endl;
#endif

    return 0;
}
