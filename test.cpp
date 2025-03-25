#include <stdio.h>

#include "fuzzy_index.hpp"

std::vector<std::string> documents = {
        "This is the first document.", 
        "This document is the second document.", 
        "And this is the third one.", 
        "Is this the first document?"};

int main(int argc, char *argv[])
{
    FuzzyIndex fi("test");
    vector<IndexData> data;

    
    int i = 0;
    for(auto s : documents) {
        data.push_back(IndexData(i, s.c_str()));
        i++;
    }
    fi.build(data);
    printf("data indexed!\n");

    string query("This is the first document");
    auto results = fi.search(query, .0); 
    printf("post search %lu results\n", results.size());
    for( auto i : results ) {
        printf("%d %.2f\n", i.id, i.distance);
//        printf("%30s %.2f\n", data[i.id].text.c_str(), i.distance);
    }
    
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
