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

    if (argc < 4) {
        printf("Usage: builder <index_dir> <artist_name> <recording_name> <release_name>\n");
        return -1;
    }
    string index_dir(argv[1]);
    string artist_name_arg(argv[2]);
    string recording_name(argv[3]);
    string release_name;
    int artist_credit_id;
    
    if (argc == 5)
        release_name = string(argv[4]);
   
    log("load artist indexes");
    ArtistIndex artist_index(index_dir);
    artist_index.load();
    
    auto artist_name = encode.encode_string(artist_name_arg); 
    if (artist_name.size() == 0) {
        printf("stupid artists are not supported yet.\n");
        return 0;
    }
    vector<IndexResult> res;
    res = artist_index.index()->search(artist_name, .5);
    printf("num results: %lu\n", res.size());
    if (res.size() == 0)
        return 0;
    for(auto & row : res) {
        printf("%d: %.2f\n", row.id, row.distance); 
    }
    artist_credit_id = res[0].id;
    
    IndexCache cache(25);
    // TODO: Enable this 
    //cache.start();
    
    RecordingIndex rec_index(index_dir);
   
    ArtistReleaseRecordingData *artist_data;
    
    artist_data = cache.get(artist_credit_id);
    if (!artist_data) {
        log("not found, loading.");
        artist_data = rec_index.load(artist_credit_id);
        cache.add(artist_credit_id, artist_data);
    }
        
    res = artist_data->recording_index->search(recording_name, .5, true);
    if (res.size() == 0) {
        printf("Not recording results.\n");
        return 0;
    }
    printf("num rec results: %lu\n", res.size());
    for(auto & row : res) {
        printf("%d: %.2f\n", row.id, row.distance); 
    }
    unsigned int recording_id = res[0].id;

    unsigned int release_id = 0;
    if (release_name.size()) {
        res = artist_data->release_index->search(release_name, .5, true);
        printf("num rel results: %lu\nrels: ", res.size());
        for(auto & row : res) {
            // TODO: Simplify this
            vector<IndexSupplementalReleaseData> supp = *artist_data->release_data;
            vector<EntityRef> release_refs = supp[row.id].release_refs;
            for(auto & ref : release_refs) 
                printf("%u (%u) ", ref.id, ref.rank);
            
            printf("\n%d: %.2f\n", row.id, row.distance); 
        }
    }
    return 0;
}