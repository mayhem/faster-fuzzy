    #pragma once
#include <stdio.h>
#include <ctime>
#include <algorithm>
#include "SQLiteCpp.h"
#include "fuzzy_index.hpp"
#include "encode.hpp"
#include "utils.hpp"
#include "artist_index.hpp"

using namespace std;

// THis error message stems from artist who have only one recording that that recording has zero encodable characters.
// this will be fixed by have stupid recording/release indexes.
// artist_credit 4455221: Recording index build error: 'no index data provided.'
// id 0 text 0artist_credit 4455221: release index build error: 'no index data provided.'


// Destructor implementation for ReleaseRecordingIndex (declared in defs.hpp)
// Must be here because FuzzyIndex is only forward-declared in defs.hpp
ReleaseRecordingIndex::~ReleaseRecordingIndex() {
    delete recording_index;
    delete release_index;
}

const char *fetch_query = R"(
      SELECT artist_credit_id   
           , release_id  
           , m.release_artist_credit_id   
           , release_name   
           , recording_id  
           , recording_name 
           , score AS rank  
        FROM mapping m  
       WHERE release_artist_credit_id = ? 
          OR artist_credit_id = ?
    ORDER BY score, m.release_id
)";

const char *fetch_recording_aliases_query = R"(
      SELECT r.id
           , ra.name
        FROM recording r
        JOIN recording_alias ra
          ON ra.recording = r.id
)";

class RecordingIndex {
    private:
        string                         index_dir, db_file; 
        EncodeSearchData               encode;
        map<unsigned int, set<string>> recording_aliases;

    public:

        RecordingIndex(const string &index_dir_) {
            index_dir = index_dir_;
            db_file = index_dir_ + string("/mapping.db");
        }
        
        ~RecordingIndex() {
        }
       
        void
        load_recording_aliases() {
            log("load recording aliases");
            try
            {
                PGconn     *conn;
                PGresult   *res;
                
                const char* db_connect = std::getenv("CANONICAL_MUSICBRAINZ_DATA_CONNECT");
                if (!db_connect || strlen(db_connect) == 0) {
                    throw std::runtime_error("CANONICAL_MUSICBRAINZ_DATA_CONNECT environment variable not set");
                }
                conn = PQconnectdb(db_connect);
                if (PQstatus(conn) != CONNECTION_OK) {
                    log("Connection to database failed: %s", PQerrorMessage(conn));
                    PQfinish(conn);
                    throw std::runtime_error("PostgreSQL connection failed");
                }
               
                res = PQexec(conn, fetch_recording_aliases_query);
                if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    std::string error_msg = "Query failed: " + std::string(PQerrorMessage(conn));
                    PQclear(res);
                    PQfinish(conn);
                    throw std::runtime_error(error_msg);
                }

                for (int i = 0; i < PQntuples(res); i++) {
                    unsigned int recording_id = atoi(PQgetvalue(res, i, 0));
                    
                    // TODO: add this to stupid recording index
                    string encoded = encode.encode_string(PQgetvalue(res, i, 1));
                    if (encoded.size())
                        recording_aliases[recording_id].insert(encoded);
                }
                
                // Clear the PGresult object to free memory
                PQclear(res);
                PQfinish(conn);
            }
            catch (exception& e)
            {
                printf("build recording aliases db exception: %s\n", e.what());
            }
        }

        ReleaseRecordingIndex
        build_recording_release_indexes(unsigned int artist_credit_id) {

            // Map to track release and recording strings and their indexes 
            map<string, unsigned int>                        release_string_index_map, recording_string_index_map;
            map<string, unsigned int>                        recording_name_to_id_map; // Track first recording_id for each encoded name
            map<string, unsigned int>                        release_name_to_id_map;   // Track first release_id for each encoded name
            map<unsigned int, vector<ReleaseRecordingLink>>  links;
                
            try
            {
                SQLite::Database    db(db_file);
                SQLite::Statement   query(db, fetch_query);
          
                query.bind(1, artist_credit_id);
                query.bind(2, artist_credit_id);
                while (query.executeStep()) {
                    unsigned int ac_id = query.getColumn(0);
                    unsigned int release_id = query.getColumn(1);
                    unsigned int release_artist_credit_id = query.getColumn(2);
                    string       release_name = query.getColumn(3);
                    unsigned int recording_id = query.getColumn(4);
                    string       recording_name = query.getColumn(5);
                    unsigned int rank  = query.getColumn(6);
                   
                    // Include rows where either the recording artist_credit_id or release artist_credit_id matches
                    if (artist_credit_id != ac_id && artist_credit_id != release_artist_credit_id)
                        continue;
                    
                    string encoded_release_name = encode.encode_string(release_name);
                    string encoded_recording_name = encode.encode_string(recording_name);
                    if (encoded_recording_name.size() == 0)
                        continue;
                    
                    unsigned int release_index;
                    try {
                        release_index = release_string_index_map.at(encoded_release_name);
                    } catch (const std::out_of_range& e) {
                        release_index = release_string_index_map.size();
                        release_string_index_map[encoded_release_name] = release_index;
                        // Store the first release_id we encounter for this encoded name
                        release_name_to_id_map[encoded_release_name] = release_id;
                    } 

                    unsigned int recording_index;
                    try {
                        recording_index = recording_string_index_map.at(encoded_recording_name);
                    } catch (const std::out_of_range& e) {
                        recording_index = recording_string_index_map.size();
                        recording_string_index_map[encoded_recording_name] = recording_index;
                        // Store the first recording_id we encounter for this encoded name
                        recording_name_to_id_map[encoded_recording_name] = recording_id;
                    } 
                    
                    ReleaseRecordingLink link = { release_index, release_id, rank, recording_index, recording_id };
                    links[recording_index].push_back(link);
                }
            }
            catch (std::exception& e)
            {
                log("build rec index db exception: %s", e.what());
            }
            
            vector<string>       recording_texts(recording_string_index_map.size());
            vector<unsigned int> recording_ids(recording_string_index_map.size());
            // Map from recording_id to its original index (for linking aliases)
            map<unsigned int, unsigned int> recording_id_to_index;
            for(auto &it : recording_string_index_map) {
                recording_texts[it.second] = it.first;
                // Use the actual recording_id from the database, not the index
                unsigned int rec_id = recording_name_to_id_map[it.first];
                recording_ids[it.second] = rec_id;
                // Store the first index we see for each recording_id
                if (recording_id_to_index.find(rec_id) == recording_id_to_index.end()) {
                    recording_id_to_index[rec_id] = it.second;
                }
            }
            
            // Add recording aliases to the index
            // Collect unique recording_ids from our data
            set<unsigned int> unique_recording_ids;
            for(auto &it : recording_name_to_id_map) {
                unique_recording_ids.insert(it.second);
            }
            
            // For each recording_id, add its aliases if they exist
            for(unsigned int rec_id : unique_recording_ids) {
                auto alias_it = recording_aliases.find(rec_id);
                if (alias_it != recording_aliases.end()) {
                    for(const string &alias : alias_it->second) {
                        // Only add if this alias text is not already in the index
                        if (recording_string_index_map.find(alias) == recording_string_index_map.end()) {
                            unsigned int new_index = recording_texts.size();
                            recording_texts.push_back(alias);
                            recording_ids.push_back(rec_id);
                            
                            // Copy links from the original recording to this alias
                            auto orig_idx_it = recording_id_to_index.find(rec_id);
                            if (orig_idx_it != recording_id_to_index.end()) {
                                auto link_it = links.find(orig_idx_it->second);
                                if (link_it != links.end()) {
                                    links[new_index] = link_it->second;
                                }
                            }
                        }
                    }
                }
            }

            FuzzyIndex *recording_index = new FuzzyIndex();
            try
            {
                recording_index->build(recording_ids, recording_texts);
            }
            catch(const std::exception& e)
            {
                log("artist_credit %d: Recording index build error: '%s'", artist_credit_id, e.what());
            }

            vector<string>       release_texts(release_string_index_map.size());
            vector<unsigned int> release_ids(release_string_index_map.size());
            for(auto &it : release_string_index_map) {
                release_texts[it.second] = it.first;
                // Use the actual release_id from the database, not the index
                release_ids[it.second] = release_name_to_id_map[it.first];
            }
            FuzzyIndex *release_index = new FuzzyIndex();
            try
            {
                release_index->build(release_ids, release_texts);
            }
            catch(const std::exception& e)
            {
                log("artist_credit %d: release index build error: '%s'", artist_credit_id, e.what());
            }
            
            // Sort each vector of ReleaseRecordingLink by release_id
            for (auto& pair : links) {
                sort(pair.second.begin(), pair.second.end(), 
                     [](const ReleaseRecordingLink& a, const ReleaseRecordingLink& b) {
                         return a.release_id < b.release_id;
                     });
            }
            
            ReleaseRecordingIndex ret(recording_index, release_index, links);
            return ret;
        }

        // Load with external DB connection (for connection reuse in server)
        ReleaseRecordingIndex *
        load(const int artist_credit_id, SQLite::Database &db) {
            FuzzyIndex                   *recording_index = new FuzzyIndex();
            FuzzyIndex                   *release_index = new FuzzyIndex();
            map<unsigned int, vector<ReleaseRecordingLink>>  links;
            try
            {
                SQLite::Statement     query(db, fetch_blob_query);
            
                query.bind(1, artist_credit_id);
                if (query.executeStep()) {
                    const void* blob_data = query.getColumn(0).getBlob();
                    size_t blob_size = query.getColumn(0).getBytes();
                    
                    std::stringstream ss;
                    ss.write(static_cast<const char*>(blob_data), blob_size);
                    ss.seekg(ios_base::beg);
                    {
                        cereal::BinaryInputArchive iarchive(ss);
                        iarchive(*recording_index, *release_index, links);
                    }
                    return new ReleaseRecordingIndex(recording_index, release_index, links);
                } else {
                    log("Cannot load index for %d", artist_credit_id);
                    delete recording_index;
                    delete release_index;
                    return nullptr;
                }
            }
            catch (std::exception& e)
            {
                log("load rec index db exception: %s", e.what());
                delete recording_index;
                delete release_index;
            }
            return nullptr;
        }

        // Load with internal DB connection (for standalone tools)
        ReleaseRecordingIndex *
        load(const int artist_credit_id) {
            SQLite::Database db(db_file);
            return load(artist_credit_id, db);
        }
};
