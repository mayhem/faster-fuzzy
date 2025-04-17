#include <stdio.h>
#include <sstream>

#include "artist_index.hpp"
#include "recording_index.hpp"
#include "mbid_mapping.hpp"
#include "index_cache.hpp"
#include "utils.hpp"
#include "encode.hpp"
#include "levenshtein.hpp"

// Jeannie & Jimmy Cheatham and The Sweet Baby Blues Band

int main(int argc, char *argv[])
{
    EncodeSearchData encode;

    if (argc != 4) {
        printf("Usage: builder <index_dir> <artist_name> <recording_name>\n");
        return -1;
    }
    string index_dir(argv[1]);
    string artist_name(argv[2]);
    string recording_name(argv[3]);
   
    log("load artist indexes");
    ArtistIndex artist_index(index_dir);
    artist_index.load();
    
    auto encoded = encode.encode_string(artist_name); 
    if (encoded.first.size() == 0) {
        printf("stupid artists are not supported yet.\n")
        return 0;
    }
    string artist_name(encoded[0]);
    string artist_remainder(encoded[2]);
    
    vector<IndexResult> res;
    res = artist_index->search(artist_name, .5, true);
    printf("num results: %lu\n", res.size());
    for(auto & row : res) {
        printf("%d: %.2f\n", row.id, row.distance); 
    }
    int artist_credit_id = 65;
    
    IndexCache cache(25);
    cache.start();
    
    RecordingIndex rec_index(index_dir);
   
    ArtistReleaseRecordingData *artist_data;
    
    artist_data = cache.get(artist_credit_id);
    if (!artist_data) {
        log("not found, loading.");
        artist_data = rec_index.load(artist_credit_id);
        cache.add(artist_credit_id, artist_data);
    }
//    std::size_t dist = lev_edit_distance(str1.size(), (const lev_byte *)str1.c_str(),
//                                         str2.size(), (const lev_byte *)str2.c_str(), 0);

//    artist_data = cache.get(artist_credit_id);
//    if (!artist_data) {
//        log("artist data not found");
//        return 0;
//    }
    
    vector<IndexResult> res;
    string recording("strangers");
    res = artist_data->recording_index->search(recording, .5, true);
    printf("num results: %lu\n", res.size());
    for(auto & row : res) {
        printf("%d: %.2f\n", row.id, row.distance); 
    }
    return 0;
}