#pragma once
#include <stdio.h>
#include <ctime>

#include <cereal/archives/binary.hpp>
#include "libpq-fe.h"
#include "SQLiteCpp.h"
#include "fuzzy_index.hpp"
#include "encode.hpp"
#include "utils.hpp"

using namespace std;

const int ARTIST_INDEX_ENTITY_ID = -1;
const int STUPID_ARTIST_INDEX_ENTITY_ID = -2;

const char *fetch_artists_query = 
    "SELECT id AS artist_credit_id, name AS artist_credit_name FROM artist_credit";

const char *insert_blob_query = 
    "INSERT INTO index_cache (entity_id, index_data) VALUES (?, ?) "
    "         ON CONFLICT(entity_id) DO UPDATE SET index_data=excluded.index_data";
const char *fetch_blob_query = 
    "SELECT index_data FROM index_cache WHERE entity_id = ?";


class ArtistIndex {
    private:
        string                         index_dir, db_file; 
        EncodeSearchData               encode;

    public:
        FuzzyIndex                     *artist_index, *stupid_artist_index;

        ArtistIndex(const string &_index_dir) {
            index_dir = _index_dir;
            db_file = _index_dir + string("/mapping.db");
            artist_index = nullptr;
            stupid_artist_index = nullptr;
        }
        
        ~ArtistIndex() {
            delete artist_index;
            delete stupid_artist_index;
        }
        
        FuzzyIndex *
        index() {
            return artist_index;
        }

        FuzzyIndex *
        stupid() {
            return stupid_artist_index;
        }
        
        // 幾何学模様 a                  Kikagaku Moyo c
        void build() {
            
            vector<unsigned int> index_ids;
            vector<string>       index_texts;

            try
            {
                auto conn = PQconnectdb("dbname=musicbrainz_db user=musicbrainz host=localhost port=5432");
//                pqxx::connection     conn("dbname=musicbrainz_db user=musicbrainz host=localhost port=5432");
//                pqxx::nontransaction txn(conn);
               
                printf("execute query\n");
//                pqxx::result result  = txn.exec(fetch_artists_query);
                
//                for (pqxx::row row : result) {
//                    // Access columns by their name or index
//                    // For example, row["id"] or row[0]
//                    index_ids.push_back(row["artist_credit_id"].as<int>());
//                    index_texts.push_back(row["artist_credit_name"].as<std::string>());
//                }

                log("fetch rows");
            }
            catch (std::exception& e)
            {
                printf("build artist db exception: %s\n", e.what());
            }
            vector<unsigned int> output_ids, stupid_ids;
            vector<string>       output_texts, output_rems, stupid_texts, stupid_rems;
                    
            log("encode data");
            encode.encode_index_data(index_ids, index_texts, output_ids, output_texts, stupid_ids, stupid_texts);
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
                    printf("save artist index db exception: %s\n", e.what());
                }
            }
            log("done building artists indexes.");
        }
        
        bool
        load_index(const int entity_id, FuzzyIndex *index) {
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
                        iarchive(*index);
                    }
                    return true;
                } else 
                    return false;
            }
            catch (std::exception& e)
            {
                printf("load artist index db exception: %s\n", e.what());
            }
            return false;
        }

        bool load() {
            artist_index = new FuzzyIndex();
            bool ret = load_index(ARTIST_INDEX_ENTITY_ID, artist_index);
            if (!ret) {
                throw length_error("failed to load artist index");
                delete artist_index;
                artist_index = nullptr;
                return false;
            }
            stupid_artist_index = new FuzzyIndex();
            return load_index(STUPID_ARTIST_INDEX_ENTITY_ID, stupid_artist_index);
        }
};