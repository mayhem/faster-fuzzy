#pragma once
#include <stdio.h>
#include <ctime>
#include "SQLiteCpp.h"
#include "fuzzy_index.hpp"
#include "encode.hpp"
#include "utils.hpp"

using namespace std;


class RecordingRef {
    public:
       unsigned int id;
       unsigned int release_id;
       unsigned int rank;
       
    RecordingRef(unsigned int id_, unsigned int release_id_, unsigned int rank_) {
        id = id_;
        release_id = release_id_;
        rank = rank_;
    }
};

class RecordingData {
    public:
       string               text_rem;
       unsigned int         id;
       vector<RecordingRef> refs;
       
    RecordingData(unsigned int id_, const string &text_rem_, const vector<RecordingRef> &refs_) {
        text_rem = text_rem_;
        id = id_;
        refs = refs_;
    }
};

class ReleaseRef {
    public:
       unsigned int id;
       unsigned int rank;
       
    ReleaseRef(unsigned int id_, unsigned int rank_) {
        id = id_;
        rank = rank;
    }
};

class TempReleaseData {
    public:
       unsigned int       id;
       string             text;
       unsigned int       rank;
       
    TempReleaseData(unsigned int id_, string &text_, unsigned int rank_) {
        id = id_;
        text= text_;
        rank = rank_;
    }
};

class ReleaseData {
    public:
       unsigned int       id;
       string             text_rem;
       vector<ReleaseRef> release_refs;
       
    ReleaseData(unsigned int id_, const string &text_rem_, const vector<ReleaseRef> &refs_) {
        id = id_;
        text_rem = text_rem_;
        release_refs = refs_;
    }
};

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
       
        pair<FuzzyIndex *, FuzzyIndex *>
        build_recording_release_indexes(unsigned int artist_credit_id) {
            map<unsigned int, vector<unsigned int>> recording_releases;
            map<string, unsigned int>               ranks;
            map<string, vector<RecordingRef>>       recording_ref;

            try
            {
                SQLite::Database    db(db_file, SQLite::OPEN_READWRITE);
                SQLite::Statement   query(db, fetch_query);
            
                query.bind(1, artist_credit_id);
                while (query.executeStep()) {
                    unsigned int artist_credit_id = query.getColumn(0);
                    unsigned int release_id = query.getColumn(1);
                    string       release_name = query.getColumn(2);
                    unsigned int recording_id = query.getColumn(3);
                    string       recording_name = query.getColumn(4);
                    unsigned int rank  = query.getColumn(5);

                    vector<string> ret = encode.encode_string(recording_name);
                    if (ret[0].size() == 0)
                        continue;
                    
                    RecordingRef ref(recording_id, release_id, rank);
                    auto iter = recording_ref.find(ret[0]);
                    if (iter == recording_ref.end()) {
                        vector<RecordingRef> vec_ref;
                        vec_ref.push_back(ref);
                        recording_ref[ret[0]] = vec_ref;
                    }
                    else
                        recording_ref[ret[0]].push_back(ref);
                        
                    auto iter2 = recording_releases.find(recording_id);
                    if (iter2 == recording_releases.end()) {
                        vector<unsigned int> release_ids;
                        release_ids.push_back(release_id);
                        recording_releases[recording_id] = release_ids;
                    }
                    else
                        recording_releases[recording_id].push_back(release_id);

                    ret = encode.encode_string(release_name);
                    if (ret[0].size()) {
                        string k = to_string(release_id) + string("-") + ret[0];
                        ranks[k] = rank;
                    }
                }
            }
            catch (std::exception& e)
            {
                printf("db exception: %s\n", e.what());
            }
        
            vector<TempReleaseData> f_release_data;
            for(auto &data : ranks) {
                size_t split_pos = data.first.find('-');
                int release_id = stoi(data.first.substr(0, split_pos));
                string text = data.first.substr(split_pos + 1);
                TempReleaseData rel(release_id, text, data.second);
                f_release_data.push_back(rel);
            }
            ranks.clear();
            
            unsigned int i = 0;
            vector<RecordingData> recording_data;
            vector<string> recording_texts;
            vector<unsigned int> recording_ids;
            for(auto &itr : recording_ref) {
                // TODO: remainder is actually first part of text
                RecordingData data(i, itr.first, itr.second);
                recording_data.push_back(data);
                recording_texts.push_back(itr.first);
                recording_ids.push_back(i);
                i++;
            }
            
            map<string, vector<ReleaseRef>> release_ref;
            for(auto &data : f_release_data) {
                ReleaseRef ref(data.id, data.rank);
                auto iter = release_ref.find(data.text);
                if (iter == release_ref.end()) {
                    vector<ReleaseRef> vec;
                    vec.push_back(ref);
                    release_ref[data.text] = vec;
                }
                else
                    release_ref[data.text].push_back(ref);
            } 

            vector<string> release_texts;
            vector<unsigned int> release_ids;
            
            // TODO: This will go out of scope and you'll need it for search. 
            vector<ReleaseData> release_data;
            i = 0;
            for(auto &it : release_ref) {
                // TODO: remainder is actually first part of text
                ReleaseData rel(i, it.first, it.second);
                release_data.push_back(rel);
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
                printf("Index build error: '%s'\n", e.what());
                recording_index = nullptr;
            }

            FuzzyIndex *release_index = new FuzzyIndex();
            try
            {
                release_index->build(release_ids, release_texts);
            }
            catch(const std::exception& e)
            {
                printf("Index build error: '%s'\n", e.what());
                release_index = nullptr;
            }
           
            pair<FuzzyIndex *, FuzzyIndex *> ind(recording_index, release_index);
            return ind;
        }
};
