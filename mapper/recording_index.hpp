#pragma once
#include <stdio.h>
#include <ctime>
#include "SQLiteCpp.h"
#include "fuzzy_index.hpp"
#include "encode.hpp"
#include "utils.hpp"

using namespace std;

const char *fetch_query = 
    "  SELECT artist_credit_id "
    "       , release_id, release_name "
    "       , recording_id, recording_name "
    "       , score "
    "    FROM mapping "
    "   WHERE artist_credit_id = ?"
    "ORDER BY score";
    
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
       
        ArtistReleaseRecordingData
        build_recording_release_indexes(unsigned int artist_credit_id) {
            map<unsigned int, vector<unsigned int>>            recording_releases;
            map<string, unsigned int>                          release_name_rank;
            map<string, pair<unsigned int, vector<EntityRef>>> recording_ref;

            try
            {
                SQLite::Database    db(db_file);
                SQLite::Statement   query(db, fetch_query);
            
                query.bind(1, artist_credit_id);
                while (query.executeStep()) {
                    unsigned int artist_credit_id = query.getColumn(0);
                    unsigned int release_id = query.getColumn(1);
                    string       release_name = query.getColumn(2);
                    unsigned int recording_id = query.getColumn(3);
                    string       recording_name = query.getColumn(4);
                    unsigned int rank  = query.getColumn(5);

                    string ret = encode.encode_string(recording_name);
                    if (ret.size() == 0)
                        continue;

                    printf("%d '%s' '%s'\n", recording_id, ret.c_str(), recording_name.c_str());
                    
                    EntityRef ref(release_id, rank);
                    auto iter = recording_ref.find(ret);
                    if (iter == recording_ref.end()) {
                        vector<EntityRef> vec_ref;
                        vec_ref.push_back(ref);
                        pair<unsigned int, vector<EntityRef>> temp = { recording_id, vec_ref };
                        recording_ref[ret] = temp;
                    }
                    else 
                        iter->second.second.push_back(ref);
                        
                    auto iter2 = recording_releases.find(recording_id);
                    if (iter2 == recording_releases.end()) {
                        vector<unsigned int> release_ids;
                        release_ids.push_back(release_id);
                        recording_releases[recording_id] = release_ids;
                    }
                    else
                        recording_releases[recording_id].push_back(release_id);

                    ret = encode.encode_string(release_name);
                    if (ret.size()) {
                        string k = to_string(release_id) + string("-") + ret;
                        release_name_rank[k] = rank;
                    }
                }
                printf("===================================\n\n");
            }
            catch (std::exception& e)
            {
                printf("build rec index db exception: %s\n", e.what());
            }
        
            vector<TempReleaseData> t_release_data;
            for(auto &data : release_name_rank) {
                size_t split_pos = data.first.find('-');
                int release_id = stoi(data.first.substr(0, split_pos));
                string text = data.first.substr(split_pos + 1);
                TempReleaseData rel(release_id, text, data.second);
                t_release_data.push_back(rel);
            }
            release_name_rank.clear();
            vector<string> recording_texts;
            vector<unsigned int> recording_ids;
            for(auto &itr : recording_ref) {
                recording_texts.push_back(itr.first);
                recording_ids.push_back(itr.second.first);
            }
            
            map<string, vector<EntityRef>> release_ref;
            for(auto &data : t_release_data) {
                EntityRef ref(data.id, data.rank);
                string k = { data.text };
                auto iter = release_ref.find(k);
                if (iter == release_ref.end()) {
                    vector<EntityRef> vec;
                    string k = { data.text };
                    vec.push_back(ref);
                    release_ref[k] = vec;
                }
                else 
                    release_ref[k].push_back(ref);
            } 

            vector<string> release_texts;
            vector<unsigned int> release_ids;
            vector<IndexSupplementalReleaseData> *release_data = new vector<IndexSupplementalReleaseData>();
            int i = 0;
            for(auto &it : release_ref) {
                IndexSupplementalReleaseData rel = {it.second};
                rel.sort_refs_by_rank();
                release_data->push_back(rel);
                release_texts.push_back(it.first);
                release_ids.push_back(i);
                i++;
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

            FuzzyIndex *release_index = new FuzzyIndex();
            try
            {
                release_index->build(release_ids, release_texts);
            }
            catch(const std::exception& e)
            {
                //printf("Index build error: '%s'\n", e.what());
            }
            
            ArtistReleaseRecordingData ret(recording_index, release_index, release_data);
            return ret;
        }

        void
        load_index(const int entity_id, FuzzyIndex *recording_index, FuzzyIndex *release_index, 
                   vector<IndexSupplementalReleaseData> *supp_data) {
            try
            {
                SQLite::Database      db(db_file);
                SQLite::Statement     query(db, fetch_blob_query);
            
                query.bind(1, entity_id);
                if (query.executeStep()) {
                    const void* blob_data = query.getColumn(0).getBlob();
                    size_t blob_size = query.getColumn(0).getBytes();
                    
                    std::stringstream ss;
                    ss.write(static_cast<const char*>(blob_data), blob_size);
                    ss.seekg(ios_base::beg);
                    {
                        cereal::BinaryInputArchive iarchive(ss);
                        iarchive(*recording_index, *release_index, *supp_data);
                    }
                    return;
                } else {
                    printf("Cannot load index for %d\n", entity_id);
                    throw std::length_error("index not found in db");
                }
            }
            catch (std::exception& e)
            {
                printf("load rec index db exception: %s\n", e.what());
            }
            return;
        }

        ArtistReleaseRecordingData *      
        load(unsigned int artist_credit_id) {
            
            FuzzyIndex *recording_index = new FuzzyIndex(); 
            FuzzyIndex *release_index = new FuzzyIndex(); 
            vector<IndexSupplementalReleaseData> *release_data = new vector<IndexSupplementalReleaseData>();
            load_index(artist_credit_id, recording_index, release_index, release_data);

            return new ArtistReleaseRecordingData(recording_index, release_index, release_data);
        }
};
