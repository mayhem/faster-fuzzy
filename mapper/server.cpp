#include <stdio.h>
#include <sstream>

#include "artist_index.hpp"
#include "recording_index.hpp"
#include "mbid_mapping.hpp"
#include "utils.hpp"

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: builder <index_dir>\n");
        return -1;
    }
    string index_dir(argv[1]);
   
    log("build artist indexes");
    ArtistIndexes artist_index(index_dir);
    artist_index.build_artist_index();

    log("build recording indexes");
    MBIDMapping mapping(index_dir);
    mapping.build_recording_indexes();

    return 0;
}
