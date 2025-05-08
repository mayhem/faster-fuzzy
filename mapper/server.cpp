#include <stdio.h>
#include <sstream>

#include "SQLiteCpp.h"

#include "search.hpp"
#include "utils.hpp"

int main(int argc, char *argv[])
{
    if (argc < 4) {
        printf("Usage: builder <index_dir> <artist_name> <recording_name> <release_name>\n");
        return -1;
    }
    string index_dir(argv[1]);
    string artist_name(argv[2]);
    string recording_name(argv[3]);
    string release_name;
    MappingSearch search(index_dir, 25);
    search.load();
    SearchResult result;
    
    result = search.search(artist_name, release_name, recording_name);
    printf("%d %d %d\n", 
        result.artist_credit_id, result.release_id, result.recording_id);
    printf("'%s'\n", result.artist_credit_name.c_str());
    printf("'%s'\n", result.recording_name.c_str());
    printf("%-8d %s %s\n", 
        result.artist_credit_id,
        result.artist_credit_mbids[0].c_str(),
        result.artist_credit_name.c_str());
    printf("%-8d %s %s\n", 
        result.release_id,
        result.release_mbid.c_str(),
        result.release_name.c_str());
    printf("%-8d %s %s\n", 
        result.recording_id,
        result.recording_mbid.c_str(),
        result.recording_name.c_str());

    return 0;
}