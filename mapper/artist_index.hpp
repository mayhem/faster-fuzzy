#pragma once
#include <stdio.h>
#include <ctime>
#include "SQLiteCpp.h"
#include "fuzzy_index.hpp"
#include "encode.hpp"
#include "utils.hpp"

using namespace std;

const int ARTIST_INDEX_ENTITY_ID = -1;
const int STUPID_ARTIST_INDEX_ENTITY_ID = -2;

const char *fetch_artists_query = 
    "SELECT DISTINCT artist_credit_id, artist_credit_name FROM mapping";
const char *insert_blob_query = 
    "INSERT INTO index_cache (entity_id, index_data) VALUES (?, ?) "
    "         ON CONFLICT(entity_id) DO UPDATE SET index_data=excluded.index_data";

class ArtistIndexes {
    private:
        EncodeSearchData encode;
        string           index_dir, db_file; 

    public:

        ArtistIndexes(const string &_index_dir) { 
            index_dir = _index_dir;
            db_file = _index_dir + string("/mapping.db");
        }
        
        ~ArtistIndexes() {
        }
        
        // 幾何学模様 a                  Kikagaku Moyo c
        void build_artist_index() {
            
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
