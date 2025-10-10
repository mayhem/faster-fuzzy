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

class RecordingIndex {
    private:
        string           index_dir, db_file; 
        EncodeSearchData encode;

    public:

        RecordingIndex(const string &index_dir_) {
            index_dir = index_dir_;
            db_file = index_dir_ + string("/mapping.db");
        }
        
        ~RecordingIndex() {
        }
       
        ReleaseRecordingIndex
        build_recording_release_indexes(unsigned int artist_credit_id) {

            // Map to track release and recording strings and their indexes 
            map<string, unsigned int>                        release_string_index_map, recording_string_index_map;
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
                    
                    // This is likely too simplistic, but for a test...
                    if (artist_credit_id != ac_id)
                        continue;
                    
                    //printf("%-40s, %-40s\n", release_name.c_str(), recording_name.c_str());

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
                    } 

                    unsigned int recording_index;
                    try {
                        recording_index = recording_string_index_map.at(encoded_recording_name);
                    } catch (const std::out_of_range& e) {
                        recording_index = recording_string_index_map.size();
                        recording_string_index_map[encoded_recording_name] = recording_index;
                    } 
                    
                    ReleaseRecordingLink link = { release_index, release_id, rank, recording_index, recording_id };
                    links[recording_index].push_back(link);
                }
            }
            catch (std::exception& e)
            {
                printf("build rec index db exception: %s\n", e.what());
            }
            
            vector<string>       recording_texts(recording_string_index_map.size());
            vector<unsigned int> recording_ids(recording_string_index_map.size());
            for(auto &it : recording_string_index_map) {
                recording_texts[it.second] = it.first;
                recording_ids[it.second] = it.second;
            }

            FuzzyIndex *recording_index = new FuzzyIndex();
            try
            {
                recording_index->build(recording_ids, recording_texts);
            }
            catch(const std::exception& e)
            {
                //printf("Index build error: '%s'\n", e.what());
            }

            vector<string>       release_texts(release_string_index_map.size());
            vector<unsigned int> release_ids(release_string_index_map.size());
            for(auto &it : release_string_index_map) {
                release_texts[it.second] = it.first;
                release_ids[it.second] = it.second;
            }
            FuzzyIndex *release_index = new FuzzyIndex();
            try
            {
                release_index->build(release_ids, release_texts);
            }
            catch(const std::exception& e)
            {
                //printf("Index build error: '%s'\n", e.what());
            }
            
            // TODO: does rank need to be saved to disk? 
            
            // Sort each vector of ReleaseRecordingLink by release_index
            for (auto& pair : links) {
                sort(pair.second.begin(), pair.second.end(), 
                     [](const ReleaseRecordingLink& a, const ReleaseRecordingLink& b) {
                         return a.release_index < b.release_index;
                     });
            }
            
            ReleaseRecordingIndex ret(recording_index, release_index, links);
            return ret;
        }

        ReleaseRecordingIndex *
        load(const int artist_credit_id) {
            FuzzyIndex                   *recording_index = new FuzzyIndex();
            FuzzyIndex                   *release_index = new FuzzyIndex();
            map<unsigned int, vector<ReleaseRecordingLink>>  links;
            try
            {
                SQLite::Database      db(db_file);
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
                    printf("Cannot load index for %d\n", artist_credit_id);
                    throw std::length_error("index not found in db");
                }
            }
            catch (std::exception& e)
            {
                printf("load rec index db exception: %s\n", e.what());
            }
            return nullptr;
        }
};
