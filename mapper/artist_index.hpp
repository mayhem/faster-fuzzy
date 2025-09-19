#pragma once
#include <stdio.h>
#include <ctime>
#include <set>
#include <cassert>
#include <vector>

#include <cereal/archives/binary.hpp>
#include "libpq-fe.h"
#include "SQLiteCpp.h"
#include "fuzzy_index.hpp"
#include "encode.hpp"
#include "utils.hpp"

using namespace std;

const int ARTIST_INDEX_ENTITY_ID = -1;
const int STUPID_ARTIST_INDEX_ENTITY_ID = -2;

// this query can include artist_credit_ids that have no recordings!
const char *fetch_artists_query = 
  "WITH acs AS ( "
  "   SELECT DISTINCT artist_credit AS artist_credit_id  "
  "     FROM recording "
  ") "
  "   SELECT acn.artist AS artist_id "
  "        , ac.id AS artist_credit_id"
  "        , acn.name AS artist_name"
  "     FROM artist_credit_name acn "
  "     JOIN artist_credit ac "
  "       ON acn.artist_credit = ac.id "
  "     JOIN acs "
  "       ON ac.id = acs.artist_credit_id "
  "      AND artist > 1 "
  "      AND artist_count = 1 "
  "UNION"
  "   SELECT aa.artist AS artist_id"
  "        , 0 AS artist_credit_id"
  "        , aa.name AS artist_alias    "
  "     FROM artist_alias aa"
  "    WHERE aa.artist > 1 "
  " ORDER BY artist_id, artist_credit_id   ";

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
       
        void
        insert_artist_credit_mapping(const map<unsigned int, set<unsigned int>> &artist_artist_credit_map) {
            try {
                SQLite::Database db(db_file, SQLite::OPEN_READWRITE);
                
                // Begin transaction for bulk insert
                SQLite::Transaction transaction(db);
                
                // Remove all existing artist credit mappings
                log("Clearing existing artist credit mappings");
                SQLite::Statement delete_stmt(db, "DELETE FROM artist_credit_mapping");
                delete_stmt.exec();
                
                log("Inserting artist credit mappings");
                size_t total_mappings = 0;
                const size_t BATCH_SIZE = 1000;
                
                // Collect all mappings first for batch processing
                vector<pair<unsigned int, unsigned int>> all_mappings;
                for (const auto& artist_entry : artist_artist_credit_map) {
                    unsigned int artist_id = artist_entry.first;
                    
                    // Convert the set to a sorted list
                    list<unsigned int> sorted_artist_credit_ids(artist_entry.second.begin(), artist_entry.second.end());
                    
                    for (unsigned int artist_credit_id : sorted_artist_credit_ids) {
                        all_mappings.push_back({artist_id, artist_credit_id});
                    }
                }
                
                // Process mappings in batches
                for (size_t i = 0; i < all_mappings.size(); i += BATCH_SIZE) {
                    size_t batch_end = min(i + BATCH_SIZE, all_mappings.size());
                    size_t batch_size = batch_end - i;
                    
                    // Build batch INSERT statement
                    string batch_sql = "INSERT INTO artist_credit_mapping (artist_id, artist_credit_id) VALUES ";
                    for (size_t j = 0; j < batch_size; j++) {
                        if (j > 0) batch_sql += ", ";
                        batch_sql += "(?, ?)";
                    }
                    
                    SQLite::Statement batch_stmt(db, batch_sql);
                    
                    // Bind parameters for the batch
                    int param_index = 1;
                    for (size_t j = i; j < batch_end; j++) {
                        batch_stmt.bind(param_index++, all_mappings[j].first);   // artist_id
                        batch_stmt.bind(param_index++, all_mappings[j].second);  // artist_credit_id
                    }
                    
                    batch_stmt.exec();
                    total_mappings += batch_size;
                }
                
                // Commit the transaction
                transaction.commit();
                log("Inserted %lu artist credit mappings", total_mappings);
                
                // Create index on artist_id column for better query performance
                log("Creating index on artist_credit_mapping.artist_id");
                db.exec("CREATE INDEX IF NOT EXISTS artist_id_ndx ON artist_credit_mapping(artist_id)");
                log("Index created successfully");
                
            } catch (std::exception& e) {
                printf("Error inserting artist credit mappings: %s\n", e.what());
            }
        }
        
        vector<string>
        encode_and_dedup_artist_names(const vector<string> &artist_names) {
            set<string> unique_artist_names;

            for(auto name : artist_names) {
                auto encoded = encode.encode_string(name);
                if (encoded.length() == 0)
                    unique_artist_names.insert(name);
                else
                    unique_artist_names.insert(encoded);
            }
            
            vector<string> result(unique_artist_names.begin(), unique_artist_names.end());
            return result;
        }
        
        // 幾何学模様 a                  Kikagaku Moyo c
        void build() {
            
            vector<unsigned int>                               index_ids;
            vector<string>                                     index_texts;
            map<unsigned int, set<unsigned int>>               artist_artist_credit_map;

            try
            {
                PGconn     *conn;
                PGresult   *res;
                
                conn = PQconnectdb("dbname=musicbrainz_db user=musicbrainz password=musicbrainz host=127.0.0.1 port=5432");
                if (PQstatus(conn) != CONNECTION_OK) {
                    printf("Connection to database failed: %s\n", PQerrorMessage(conn));
                    PQfinish(conn);
                    return;
                }
               
                res = PQexec(conn, fetch_artists_query);
                if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    printf("Query failed: %s\n", PQerrorMessage(conn));
                    PQclear(res);
                    PQfinish(conn);
                    return;
                }

                log("fetch rows");
                unsigned int         last_artist_id = 0;
                vector<string>       artist_names;
                for (int i = 0; i < PQntuples(res) + 1; i++) {
                    unsigned  artist_id, artist_credit_id;
                    string artist_name;

                    if (i >= PQntuples(res)) 
                        break;

                    artist_id = atoi(PQgetvalue(res, i, 0));
                    artist_credit_id = atoi(PQgetvalue(res, i, 1));
                    artist_name = PQgetvalue(res, i, 2);
                    if (last_artist_id != 0 && last_artist_id != artist_id) {
                        
                        auto dedup_names = encode_and_dedup_artist_names(artist_names);
                        artist_names.clear();

                        for(auto it : dedup_names) {
                            index_ids.push_back(last_artist_id);
                            index_texts.push_back(it);
                        }
                    }
                    
                    artist_names.push_back(artist_name);
                    // artist_credit_ids are true artist aliases.
                    if (artist_credit_id) 
                        artist_artist_credit_map[artist_id].insert(artist_credit_id);
                    
                    last_artist_id = artist_id;
                }
            
                // Clear the PGresult object to free memory
                PQclear(res);
            }
            catch (std::exception& e)
            {
                printf("build artist db exception: %s\n", e.what());
            }
            vector<unsigned int> output_ids, stupid_ids;
            vector<string>       output_texts, output_rems, stupid_texts, stupid_rems;

            insert_artist_credit_mapping(artist_artist_credit_map);
                    
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
                log("artist index size: %lu bytes", ss.str().length());
           
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
