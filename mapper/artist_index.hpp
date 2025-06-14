#pragma once
#include <stdio.h>
#include <ctime>
#include <set>
#include <cassert>

#include <cereal/archives/binary.hpp>
#include "libpq-fe.h"
#include "SQLiteCpp.h"
#include "fuzzy_index.hpp"
#include "encode.hpp"
#include "utils.hpp"

using namespace std;

const int ARTIST_INDEX_ENTITY_ID = -1;
const int STUPID_ARTIST_INDEX_ENTITY_ID = -2;

// TODO: Artist index results could give the same artist_credit_id twice -- don't carry out two searches!
const char *fetch_artists_query = 
    " WITH acs AS ( "
    "       SELECT DISTINCT artist_credit AS artist_credit_id "
    "         FROM recording "
    ")"
    "       SELECT a.id AS artist_id"
    "            , a.name AS artist_name"
    "            , ac.id AS artist_credit_id"
    "            , ac.name AS artist_credit_name"
    "            , ac.artist_count"
    "         FROM musicbrainz.artist_credit ac"
    "         JOIN musicbrainz.artist_credit_name acn"
    "           ON acn.artist_credit = ac.id"
    "         JOIN artist a"
    "           ON acn.artist = a.id"
    "         JOIN acs "
    "           ON ac.id = acs.artist_credit_id "
    "        WHERE a.id > 1" 
//    "          AND a.id = 1053737"
    "     ORDER BY artist_id, artist_credit_id";
//    "          AND a.id > 1117030 AND a.id < 1117100"
//    "          AND a.id < 1000"

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
            
            vector<unsigned int>                               index_ids;
            vector<string>                                     index_texts;
            map<unsigned int, vector<string>>                  alias_map;

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
                unsigned int         last_id = 0;
                unsigned int         saved_artist_credit_id = 0;
                string               last_text;
                vector<unsigned int> alias_ids;
                vector<string>       alias_texts;
                for (int i = 0; i < PQntuples(res) + 1; i++) {
                    int id, artist_credit_id, artist_count;
                    string text, artist_credit_name;

                    if (i < PQntuples(res)) {
                        id = atoi(PQgetvalue(res, i, 0));
                        text = PQgetvalue(res, i, 1);
                        artist_credit_id = atoi(PQgetvalue(res, i, 2));
                        artist_credit_name = PQgetvalue(res, i, 3);
                        artist_count = atoi(PQgetvalue(res, i, 4));
                        if (last_id == 0) {
                            last_id = id;
                            saved_artist_credit_id = artist_credit_id;
                        }
                    }
                    else 
                        id = 0;

                    if (id != last_id) {
                        size_t i;
                        for(i = 0; i < alias_ids.size(); i++) {
                            alias_map[alias_ids[i]] = alias_texts;
                            printf("aliases: %d -> ", alias_ids[i]);
                            int j = 0;
                            for(auto &meh : alias_texts) {
                                printf("'%s' ", meh.c_str());
                                j++;
                            }
                            if (j > 0)
                                printf("\n");
                        }
                        if (i > 0)
                            printf("\n");
                        if (i == PQntuples(res) - 1) 
                            break;
                        alias_ids.clear();
                        alias_texts.clear();
                        
                        // Now look at the first row of the next artist 
                        saved_artist_credit_id = artist_credit_id; 
                    }
                    
                    // Check to see why the other artsts are not here
                    
                    printf("raw: %d '%s' -> %d '%s'\n", id, text.c_str(), artist_credit_id, artist_credit_name.c_str());
                    if (artist_credit_id != id && artist_count == 1 && text != artist_credit_name) {
                        alias_ids.push_back(artist_credit_id);
                        alias_texts.push_back(artist_credit_name);
                    }
                    index_ids.push_back(artist_credit_id);
                    index_texts.push_back(artist_credit_name);
                    
                    last_id = id;
                    last_text = text;
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
                    
            log("encode data");
            encode.encode_index_data(index_ids, index_texts, alias_map, output_ids, output_texts, stupid_ids, stupid_texts);
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
