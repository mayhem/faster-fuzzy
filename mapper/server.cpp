#include <stdio.h>
#include <sstream>

#include "SQLiteCpp.h"

#include "artist_index.hpp"
#include "recording_index.hpp"
#include "mbid_mapping.hpp"
#include "index_cache.hpp"
#include "utils.hpp"
#include "encode.hpp"
#include "levenshtein.hpp"

// TODO: Implement DB connection re-use
// Jeannie & Jimmy Cheatham and The Sweet Baby Blues Band

const char *fetch_metadata_query_0 = 
    "  WITH input(ind, artist_credit_id, release_id, recording_id) AS ( "
    "       VALUES ";
const char *fetch_metadata_query_1 = 
    "  )"
    "  SELECT ind, artist_mbids, artist_credit_name, release_mbid, release_name, recording_mbid, recording_name "
    "    FROM mapping "
    "    JOIN input "
    "      ON input.artist_credit_id = mapping.artist_credit_id AND "
    "         input.release_id = mapping.release_id AND "
    "         input.recording_id = mapping.recording_id";
    
vector<string> split(const std::string& input) {
    vector<std::string> result;
    stringstream        ss(input);
    string              token;

    while (getline(ss, token, ',')) {
        result.push_back(token);
    }

    return result;
}

void
fetch_metadata(const string &index_dir, vector<SearchResult> &results) {
    string db_file = index_dir + string("/mapping.db");
   
    string query = string(fetch_metadata_query_0);
    string values;
    for (size_t i = 0; i < results.size(); i++) {
        if (i > 0) {
            query += string(", ");
            values += string(", ");
        } 
        query += string("(?, ?, ?, ?)");
        values += string("(");
        values += to_string(results[i].artist_credit_id);
        values += string(",");
        values += to_string(results[i].release_id);
        values += string(",");
        values += to_string(results[i].recording_id);
        values += string(")\n");
    }
    query += string(fetch_metadata_query_1);

    try
    {
        SQLite::Database    db(db_file);
        SQLite::Statement   db_query(db, query);
        
        int index = 1;
        for (const auto& result : results) {
            db_query.bind(index, (index++)-1);
            db_query.bind(index++, result.artist_credit_id);
            db_query.bind(index++, result.release_id);
            db_query.bind(index++, result.recording_id);
        }

        while (db_query.executeStep()) {
            index = db_query.getColumn(0).getInt();
            results[index].artist_credit_mbids = split(db_query.getColumn(1).getString());
            results[index].artist_credit_name = db_query.getColumn(2).getString();
            results[index].release_mbid = db_query.getColumn(3).getString();
            results[index].release_name = db_query.getColumn(4).getString();
            results[index].recording_mbid = db_query.getColumn(5).getString();
            results[index].recording_name = db_query.getColumn(6).getString();
        }
    }
    catch (std::exception& e)
    {
        printf("build artist db exception: %s\n", e.what());
    }
}

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
   
    ArtistIndex artist_index(index_dir);
    artist_index.load();
    
    auto artist_name = encode.encode_string(artist_name_arg); 
    if (artist_name.size() == 0) {
        printf("stupid artists are not supported yet.\n");
        return 0;
    }
    vector<IndexResult> res;
    res = artist_index.index()->search(artist_name, .7);
    if (res.size() == 0)
        return 0;
    artist_credit_id = res[0].id;
    
    IndexCache cache(25);
    // TODO: Enable this 
    //cache.start();
    
    RecordingIndex rec_index(index_dir);
    ArtistReleaseRecordingData *artist_data;
    
    artist_data = cache.get(artist_credit_id);
    if (!artist_data) {
        artist_data = rec_index.load(artist_credit_id);
        cache.add(artist_credit_id, artist_data);
    }
        
    auto recording_name_encoded = encode.encode_string(recording_name); 
    if (recording_name_encoded.size() == 0) {
        printf("recording name contains no word characters.\n");
        return 0;
    }
    res = artist_data->recording_index->search(recording_name_encoded, .7);
    if (res.size() == 0) {
        printf("Not recording results.\n");
        return 0;
    }
    unsigned int recording_id = res[0].id;

    unsigned int release_id = 0;
    if (release_name.size()) {
        auto release_name_encoded = encode.encode_string(release_name); 
        if (release_name_encoded.size() == 0) {
            printf("warning: release name contains no word characters, ignoring.\n");
            return 0;
        }
        res = artist_data->release_index->search(release_name_encoded, .7);
        if (res.size()) {
            IndexResult &result = res[0];
            EntityRef &ref = (*artist_data->release_data)[result.id].release_refs[0];
            release_id = ref.id;
        }
    }
    
    if (artist_credit_id == 0 || release_id == 0 || recording_id == 0) {
        printf("%d, %d, %d\n", artist_credit_id, release_id, recording_id);
        printf("not all data resolved.\n");
        return 0;
    }
    
    SearchResult sres(artist_credit_id, release_id, recording_id);
    vector<SearchResult> sresults;
    sresults.push_back(sres);
   
    fetch_metadata(index_dir, sresults);
    for(auto &r : sresults) {
        printf("%-40s: %s %d\n", 
            r.artist_credit_name.substr(0, 39).c_str(),
            r.artist_credit_mbids[0].c_str(),
            r.artist_credit_id);
        printf("%-40s: %s %d\n", 
            r.release_name.substr(0, 39).c_str(),
            r.release_mbid.c_str(),
            r.release_id);
        printf("%-40s: %s %d\n", 
            r.recording_name.substr(0, 39).c_str(),
            r.recording_mbid.c_str(),
            r.recording_id);
    }
    return 0;
}