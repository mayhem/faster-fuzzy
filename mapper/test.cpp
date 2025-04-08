#include <stdio.h>
#include <sstream>

#include "artist_index.hpp"

vector<string> documents = { "this is a test", "cam drive rump"};
vector<unsigned int> ids = { 0, 1 };

int main(int argc, char *argv[])
{
#if 0
    if (argc != 2) {
        printf("Usage: builder <index_dir>\n");
        return -1;
    }
    string index_dir(argv[1]);
    ArtistIndexes builder(index_dir);
    builder.build_artist_index();
#endif

    FuzzyIndex fi;
    fi.build(ids, documents);
    
#if 0
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
#endif

    string query("This is a nest");
    auto results = fi.search(query, .0); 
    printf("%lu results:\n", results.size());
    for( auto i : results ) {
        printf("  %d %.2f\n", i.id, i.distance);
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