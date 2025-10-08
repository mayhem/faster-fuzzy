#include <stdio.h>

#include "artist_index.hpp"
#include "mbid_mapping.hpp"
#include "utils.hpp"

int main(int argc, char *argv[])
{
    bool skip_artists = false;
    string index_dir;
    
    if (argc < 2 || argc > 3) {
        printf("Usage: mapping_create [--skip-artists] <index_dir>\n");
        printf("  --skip-artists  Skip building artist indexes\n");
        return -1;
    }
    
    // Parse arguments
    if (argc == 3) {
        if (string(argv[1]) == "--skip-artists") {
            skip_artists = true;
            index_dir = string(argv[2]);
        } else {
            printf("Unknown option: %s\n", argv[1]);
            printf("Usage: mapping_create [--skip-artists] <index_dir>\n");
            return -1;
        }
    } else {
        index_dir = string(argv[1]);
    }
   
    if (!skip_artists) {
        log("build artist indexes");
        ArtistIndex *artist_index = new ArtistIndex(index_dir);
        artist_index->build();
        // clean up to free memory
        delete artist_index;
    } else {
        log("skipping artist indexes (--skip-artists specified)");
    }

    log("build recording indexes");
    MBIDMapping mapping(index_dir);
    mapping.build_recording_indexes();

    return 0;
}