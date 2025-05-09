#include <stdio.h>
#include <sstream>

#include "SQLiteCpp.h"

#include "search.hpp"
#include "utils.hpp"

string
make_comma_sep_string(const vector<string> &str_array) {
    string ret; 
    int index = 0;
    for(auto &it : str_array) {
        if (index > 0)
            ret += string(",");
                
        ret += it;
        index += 1;
    }
    return ret;
}

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
   
    if (argc == 5)
        release_name = string(argv[4]);

    MappingSearch search(index_dir, 25);
    search.load();
    SearchResult *result;
    
    result = search.search(artist_name, release_name, recording_name);
    if (result) {
        printf("%-8d %s %s\n", 
            result->artist_credit_id,
            result->artist_credit_mbids[0].c_str(),
            result->artist_credit_name.c_str());
        printf("%-8d %s %s\n", 
            result->release_id,
            result->release_mbid.c_str(),
            result->release_name.c_str());
        printf("%-8d %s %s\n", 
            result->recording_id,
            result->recording_mbid.c_str(),
            result->recording_name.c_str());
        string mbids = make_comma_sep_string(result->artist_credit_mbids);
        printf("    { \"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\" },\n", artist_name.c_str(), release_name.c_str(), recording_name.c_str(),
                                                            mbids.c_str(), result->release_mbid.c_str(), result->recording_mbid.c_str());
        
        delete result;
    }
    else
        printf("No match\n");

    return 0;
}