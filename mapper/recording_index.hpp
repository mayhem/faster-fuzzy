#pragma once
#include <stdio.h>
#include <ctime>
#include "SQLiteCpp.h"
#include "fuzzy_index.hpp"
#include "encode.hpp"
#include "utils.hpp"

using namespace std;

const char *fetch_query = 
    "SELECT artist_credit_id "
    "     , release_id, release_name "
    "     , recording_id, recording_name "
    "     , score "
    "  FROM mapping "
    " WHERE artist_credit_id = ?";
    
class RecordingIndexes {
    private:
        EncodeSearchData encode;
        string           index_dir, db_file; 

    public:

        RecordingIndexes(const string &_index_dir) { 
            index_dir = _index_dir;
            db_file = _index_dir + string("/mapping.db");
        }
        
        ~RecordingIndexes() {
        }
       
        pair<pair<FuzzyIndex *, vector<IndexSupplementalData> *>, pair<FuzzyIndex *, vector<IndexSupplementalReleaseData> *>>
        build_recording_release_indexes(unsigned int artist_credit_id) {
            map<unsigned int, vector<unsigned int>>                             recording_releases;
            map<string, pair<string, unsigned int>>                             ranks;
            map<pair<string, string>, pair<unsigned int, vector<EntityRef>>> recording_ref;

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

                    pair<string, string> ret = encode.encode_string(recording_name);
                    if (ret.first.size() == 0)
                        continue;
                    
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
                    if (ret.first.size()) {
                        string k = to_string(release_id) + string("-") + ret.first;
                        pair<string, unsigned int> temp = { ret.second, rank};
                        ranks[k] = temp;
                    }
                }
            }
            catch (std::exception& e)
            {
                printf("db exception: %s\n", e.what());
            }
        
            vector<TempReleaseData> t_release_data;
            for(auto &data : ranks) {
                size_t split_pos = data.first.find('-');
                int release_id = stoi(data.first.substr(0, split_pos));
                string text = data.first.substr(split_pos + 1);
                TempReleaseData rel(release_id, text, data.second.first, data.second.second);
                t_release_data.push_back(rel);
            }
            ranks.clear();
            
            unsigned int i = 0;
            vector<IndexSupplementalData> *supp_recording_data = new vector<IndexSupplementalData>();
            vector<string> recording_texts;
            vector<unsigned int> recording_ids;
            for(auto &itr : recording_ref) {
                IndexSupplementalData data = { itr.first.second, itr.second.first };
                supp_recording_data->push_back(data);
                recording_texts.push_back(itr.first.first);
                recording_ids.push_back(i);
                i++;
            }
            
            map<pair<string, string>, vector<EntityRef>> release_ref;
            for(auto &data : t_release_data) {
                EntityRef ref(data.id, data.rank);
                pair<string, string> k = { data.text, data.remainder };
                auto iter = release_ref.find(k);
                if (iter == release_ref.end()) {
                    vector<EntityRef> vec;
                    pair<string, string> k = { data.text, data.remainder };
                    vec.push_back(ref);
                    release_ref[k] = vec;
                }
                else
                    release_ref[k].push_back(ref);
            } 

            vector<string> release_texts;
            vector<unsigned int> release_ids;
            vector<IndexSupplementalReleaseData> *release_data = new vector<IndexSupplementalReleaseData>();
            i = 0;
            for(auto &it : release_ref) {
                IndexSupplementalReleaseData rel(i, it.first.second, it.second);
                release_data->push_back(rel);
                release_texts.push_back(it.first.first);
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
           
            pair<pair<FuzzyIndex *, vector<IndexSupplementalData> *>,
                 pair<FuzzyIndex *, vector<IndexSupplementalReleaseData> *>> ind = { { recording_index, supp_recording_data}, 
                                                                                      { release_index, release_data } };
            return ind;
        }
};
