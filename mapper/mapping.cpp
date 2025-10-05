#include <stdio.h>

#include "artist_index.hpp"
#include "mbid_mapping.hpp"
#include "utils.hpp"

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: mapping_create <index_dir>\n");
        return -1;
    }
    string index_dir(argv[1]);
   
//    log("build artist indexes");
//    ArtistIndex *artist_index = new ArtistIndex(index_dir);
//    artist_index->build();
//    // clean up to free memory
//    delete artist_index;

    log("build recording indexes");
    MBIDMapping mapping(index_dir);
    mapping.build_recording_indexes();

    return 0;
}