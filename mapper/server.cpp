#include <stdio.h>
#include <sstream>

#include "artist_index.hpp"
#include "recording_index.hpp"
#include "mbid_mapping.hpp"
#include "index_cache.hpp"
#include "utils.hpp"

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: builder <index_dir>\n");
        return -1;
    }
    string index_dir(argv[1]);
   
    log("load artist indexes");
    ArtistIndex artist_index(index_dir);
    artist_index.load();
    log("artist indexes loaded");
    
    string q("portishead");
    artist_index.search(q);
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