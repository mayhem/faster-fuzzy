#include <stdio.h>
#include <sstream>

#include "artist_index.hpp"
#include "recording_index.hpp"

vector<string> documents = { "this is a test", "cam drive rump"};
vector<unsigned int> ids = { 0, 1 };

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: builder <index_dir>\n");
        return -1;
    }
    string index_dir(argv[1]);
    ArtistIndexes builder(index_dir);
    builder.build_artist_index();

#if 0
    std::stringstream ss;
    {
        cereal::BinaryOutputArchive oarchive(ss);
        oarchive(*fi);
    }
    delete fi;
    fi = nullptr;
    ss.seekg(ios_base::beg);
    {
        cereal::BinaryInputArchive iarchive(ss);
        iarchive(reloaded);
    }

    string query("portished");
    auto results = reloaded.search(query, .6); 
    printf("%lu results:\n", results.size());
    for( auto i : results ) {
        printf("  %d %.2f\n", i.id, i.distance);
    }
#endif

#if 0
    FuzzyIndex fi;
    fi.build(ids, documents);
    
//    std::stringstream ss;
//    {
//        cereal::BinaryOutputArchive oarchive(ss);
//        oarchive(fi);
//    }
//   
//    ss.seekg(ios_base::beg);
//    {
//        cereal::BinaryInputArchive iarchive(ss);
//        iarchive(reloaded);
//    }

    string query("This is a nest");
    auto results = fi.search(query, .0); 
    printf("%lu results:\n", results.size());
    for( auto i : results ) {
        printf("  %d %.2f\n", i.id, i.distance);
    }
#endif
    
    return 0;
}