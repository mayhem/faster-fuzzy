#include <stdio.h>
#include <sstream>

#include "builder.hpp"
#include "encode.hpp"


int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: builder <index_dir>\n");
        return -1;
    }
    string index_dir(argv[1]);
    IndexBuilder builder(index_dir);
    
    builder.build();

#if 0
//    int i = 0;
//    for(auto s : documents) {
//        data.push_back(IndexData(i, s.c_str()));
//        i++;
//    }
//                auto ret = encoder.encode_string(entry.text);
//                if (ret[0].size() == 0) {
//                    ret = encoder.encode_string_for_stupid_artists(entry.text);
//                    if (ret[0].size() == 0) {
//                    
//                }
                
    fi.build(data);
    
    std::stringstream ss;
    {
        cereal::BinaryOutputArchive oarchive(ss);
        oarchive(fi);
    }
   
    ss.seekg(ios_base::beg);
    {
        cereal::BinaryInputArchive iarchive(ss);
        iarchive(reloaded);
    }

    string query("This is the first document");
    auto results = reloaded.search(query, .0); 
    printf("%lu results:\n", results.size());
    for( auto i : results ) {
        printf("  %d %.2f\n", i.id, i.distance);
//        printf("%30s %.2f\n", data[i.id].text.c_str(), i.distance);
    }
    
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