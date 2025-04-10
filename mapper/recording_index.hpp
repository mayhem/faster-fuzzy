#pragma once
#include <stdio.h>
#include <ctime>
#include "SQLiteCpp.h"
#include "fuzzy_index.hpp"
#include "encode.hpp"
#include "utils.hpp"

using namespace std;

class RecordingData {
    public:
       string text;
       unsigned int id;
       
    RecordingData(unsigned int id_, string &text_) {
        text= text_;
        id = id_;
    }
};

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

class ReleaseData {
    public:
       string text;
       unsigned int id;
       unsigned int rank;
       
    ReleaseData(unsigned int id_, string &text_, unsigned int rank_) {
        text= text_;
        id = id_;
        rank = rank;
    }
};

const char *fetch_query = 
    "SELECT artist_credit_id, artist_credit_name "
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
        
        void collect_artist_data(unsigned int artist_credit_id) {
            vector<RecordingData>             recording_data;
            map<unsigned int, unsigned int>   recording_releases;
            map<string, unsigned int>         ranks;
            map<string, vector<RecordingRef>> recording_ref;

            for(;;) {
                try
                {
                    SQLite::Database    db(db_file, SQLite::OPEN_READWRITE);
                    SQLite::Statement   query(db, fetch_query);
                
                    query.bind(1, artist_credit_id);
                    while (query.executeStep()) {
                        vector<string> ret = encode.encode_string(query.getColumn(5));
                        if (ret[0].size() == 0)
                            continue;
                        
                        RecordingRef ref(query.getColumn(4), query.getColumn(2), query.getColumn(6));
                        auto iter = recording_ref.find(ret[0]);
                        if (iter == recording_ref.end()) {
                            vector<RecordingRef> vec_ref;
                            vec_ref.push_back(ref);
                            recording_ref[ret[0]] = vec_ref;
                        }
                        else
                            recording_ref[ret[0]].push_back(ref);
                    }
                }
                catch (std::exception& e)
                {
                    printf("db exception: %s\n", e.what());
                }
            }
        }
        
        void build_index() {
            
            vector<unsigned int> index_ids;
            vector<string>       index_texts;

            try
            {
                SQLite::Database    db(db_file);
                log("execute query");
                SQLite::Statement   query(db, fetch_artists_query);
        
                log("fetch rows");
                while (query.executeStep()) {
                    index_ids.push_back(query.getColumn(0));
                    index_texts.push_back(query.getColumn(1));
                }
            }
            catch (std::exception& e)
            {
                printf("db exception: %s\n", e.what());
            }
            vector<unsigned int> output_ids, stupid_ids;
            vector<string>       output_texts, output_rems, stupid_texts, stupid_rems;
                    
            log("encode data");
            encode.encode_index_data(index_ids, index_texts, output_ids, output_texts, output_rems,
                                                             stupid_ids, stupid_texts, stupid_rems);
            log("%lu items in index", output_ids.size());
            {
                FuzzyIndex *index = new FuzzyIndex();
                log("build artist index");
                index->build(output_ids, output_texts);

                log("serialize artist index");
                std::stringstream ss;
                {
                    cereal::BinaryOutputArchive oarchive(ss);
                    oarchive(*index);
                }
                log("artist index size: %lu", ss.str().length());
           
                std::stringstream sss;
                if (stupid_ids.size()) {
                    FuzzyIndex *stupid_index = new FuzzyIndex();
                    stupid_index->build(stupid_ids, stupid_texts);
                    {
                        cereal::BinaryOutputArchive oarchive(sss);
                        oarchive(*stupid_index);
                    }
                    log("stupid artist index size: %lu", sss.str().length());
                }

                try
                {
                    SQLite::Database    db(db_file, SQLite::OPEN_READWRITE);
                    SQLite::Statement   query(db, insert_blob_query);
                
                    log("save artist index");
                    query.bind(1, ARTIST_INDEX_ENTITY_ID);
                    query.bind(2, (const char *)ss.str().c_str(), (int32_t)ss.str().length());
                    query.exec();

                    SQLite::Statement   query2(db, insert_blob_query);
                    if (stupid_ids.size()) {
                        log("save stupid artist index");
                        query2.bind(1, STUPID_ARTIST_INDEX_ENTITY_ID);
                        query2.bind(2, (const char *)sss.str().c_str(), (int32_t)sss.str().length());
                        query2.exec();
                    }
                }
                catch (std::exception& e)
                {
                    printf("db exception: %s\n", e.what());
                }
            }
            log("done building artists indexes.");
        }
};
