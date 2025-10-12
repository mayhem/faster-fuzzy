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

const int SINGLE_ARTIST_INDEX_ENTITY_ID = -1;
const int MULTIPLE_ARTIST_INDEX_ENTITY_ID = -2;
const int STUPID_ARTIST_INDEX_ENTITY_ID = -3;

// Fetch artist names and aliases via the artist_credits and artist aliases for artist credits with artist_count = 1
const char *fetch_artists_query = R"(
WITH acs AS ( 
   SELECT DISTINCT artist_credit_id  
     FROM mapping.canonical_musicbrainz_data_release_support 
    WHERE artist_credit_id > 1
) 
   SELECT acn.artist AS artist_id 
        , ac.id AS artist_credit_id
        , acn.name AS artist_name
     FROM artist_credit_name acn 
     JOIN artist_credit ac 
       ON acn.artist_credit = ac.id 
     JOIN acs 
       ON ac.id = acs.artist_credit_id 
      AND artist > 1
      AND artist_count = 1 
UNION
   SELECT aa.artist AS artist_id
        , 0 AS artist_credit_id
        , aa.name AS artist_alias    
     FROM artist_alias aa
    WHERE aa.artist > 1 
 ORDER BY artist_id, artist_credit_id)";


// Fetch artist names for artist credits with artist_count = 1
const char *fetch_single_artists_query = R"(
WITH acs AS ( 
   SELECT DISTINCT artist_credit_id  
     FROM mapping.canonical_musicbrainz_data_release_support 
    WHERE artist_credit_id > 1
) 
   SELECT ac.id AS artist_credit_id
        , ac.name AS artist_name
     FROM artist_credit ac 
     JOIN acs 
       ON ac.id = acs.artist_credit_id 
     JOIN artist a
      AND artist_count = 1 
 ORDER BY artist_credit_id)";

// Fetch artist names for artist credits with artist_count > 1
const char *fetch_multiple_artists_query = R"(
WITH acs AS ( 
   SELECT DISTINCT artist_credit_id  
     FROM mapping.canonical_musicbrainz_data_release_support 
    WHERE artist_credit_id > 1
) 
   SELECT ac.id AS artist_credit_id
        , ac.name AS artist_name
     FROM artist_credit ac 
     JOIN acs 
       ON ac.id = acs.artist_credit_id 
      AND artist_count > 1 
 ORDER BY artist_credit_id)";

const char *insert_blob_query = R"(
    INSERT INTO index_cache (entity_id, index_data) VALUES (?, ?)
             ON CONFLICT(entity_id) DO UPDATE SET index_data=excluded.index_data)";
const char *fetch_blob_query = 
    "SELECT index_data FROM index_cache WHERE entity_id = ?";


class ArtistIndex {
    private:
        string                                    index_dir, db_file; 
        EncodeSearchData                          encode;

        // Single artist index
        vector<unsigned int>                      single_artist_credit_ids;
        vector<string>                            single_artist_credit_texts;

        // Multiple artist index
        vector<unsigned int>                      multiple_artist_credit_ids;
        vector<string>                            multiple_artist_credit_texts;

    public:
        FuzzyIndex                               *single_artist_index, *multiple_artist_index, *stupid_artist_index;

        ArtistIndex(const string &_index_dir) {
            index_dir = _index_dir;
            db_file = _index_dir + string("/mapping.db");
            single_artist_index = nullptr;
            multiple_artist_index = nullptr;
            stupid_artist_index = nullptr;
        }
        
        ~ArtistIndex() {
            delete single_artist_index;
            delete stupid_artist_index;
            delete multiple_artist_index;
        }
#if 0        
        void
        insert_artist_credit_mapping(map<unsigned int, set<unsigned int>> &artist_credit_map) {
            try {
                SQLite::Database db(db_file, SQLite::OPEN_READWRITE);
                
                // Begin transaction for bulk insert
                SQLite::Transaction transaction(db);
                
                // Remove all existing artist credit mappings
                SQLite::Statement delete_stmt(db, "DELETE FROM artist_credit_mapping");
                delete_stmt.exec();
                
                size_t total_mappings = 0;
                const size_t BATCH_SIZE = 1000;
                
                // Collect all mappings first for batch processing
                vector<pair<unsigned int, unsigned int>> all_mappings;
                for (const auto& artist_entry : artist_credit_map) {
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
                vector<pair<unsigned int, unsigned int>>().swap(all_mappings);
                
                // Commit the transaction
                transaction.commit();
                
                // Create index on artist_id column for better query performance
                db.exec("CREATE INDEX IF NOT EXISTS artist_id_ndx ON artist_credit_mapping(artist_id)");
                
            } catch (std::exception& e) {
                printf("Error inserting artist credit mappings: %s\n", e.what());
            }
        }
#endif        
        bool
        is_transliterated(const string &artist_name, const string &artist_sortname) {
            bool has_latin = false;
            bool has_non_latin = false;
            
            // Check artist_name for mix of Latin and non-Latin characters
            for (size_t i = 0; i < artist_name.length(); ) {
                uint32_t codepoint = 0;
                int byte_count = 0;
                
                // Parse UTF-8 character
                unsigned char byte = artist_name[i];
                if (byte < 0x80) {
                    // Single byte character (ASCII)
                    codepoint = byte;
                    byte_count = 1;
                } else if ((byte & 0xE0) == 0xC0) {
                    // Two byte character
                    if (i + 1 >= artist_name.length()) break;
                    codepoint = ((byte & 0x1F) << 6) | (artist_name[i + 1] & 0x3F);
                    byte_count = 2;
                } else if ((byte & 0xF0) == 0xE0) {
                    // Three byte character
                    if (i + 2 >= artist_name.length()) break;
                    codepoint = ((byte & 0x0F) << 12) | ((artist_name[i + 1] & 0x3F) << 6) | (artist_name[i + 2] & 0x3F);
                    byte_count = 3;
                } else if ((byte & 0xF8) == 0xF0) {
                    // Four byte character
                    if (i + 3 >= artist_name.length()) break;
                    codepoint = ((byte & 0x07) << 18) | ((artist_name[i + 1] & 0x3F) << 12) | 
                               ((artist_name[i + 2] & 0x3F) << 6) | (artist_name[i + 3] & 0x3F);
                    byte_count = 4;
                } else {
                    // Invalid UTF-8, skip this byte
                    i++;
                    continue;
                }
                
                // Check if character is in Latin range (U+0000 to U+024F)
                if (codepoint <= 0x024F) {
                    has_latin = true;
                } else {
                    has_non_latin = true;
                }
                
                i += byte_count;
            }
            
            // If artist_name doesn't have both Latin and non-Latin, it's not transliterated
            if (!has_latin || !has_non_latin) {
                return false;
            }
            
            // Check artist_sortname contains ONLY Latin characters
            for (size_t i = 0; i < artist_sortname.length(); ) {
                uint32_t codepoint = 0;
                int byte_count = 0;
                
                // Parse UTF-8 character
                unsigned char byte = artist_sortname[i];
                if (byte < 0x80) {
                    // Single byte character (ASCII)
                    codepoint = byte;
                    byte_count = 1;
                } else if ((byte & 0xE0) == 0xC0) {
                    // Two byte character
                    if (i + 1 >= artist_sortname.length()) break;
                    codepoint = ((byte & 0x1F) << 6) | (artist_sortname[i + 1] & 0x3F);
                    byte_count = 2;
                } else if ((byte & 0xF0) == 0xE0) {
                    // Three byte character
                    if (i + 2 >= artist_sortname.length()) break;
                    codepoint = ((byte & 0x0F) << 12) | ((artist_sortname[i + 1] & 0x3F) << 6) | (artist_sortname[i + 2] & 0x3F);
                    byte_count = 3;
                } else if ((byte & 0xF8) == 0xF0) {
                    // Four byte character
                    if (i + 3 >= artist_sortname.length()) break;
                    codepoint = ((byte & 0x07) << 18) | ((artist_sortname[i + 1] & 0x3F) << 12) | 
                               ((artist_sortname[i + 2] & 0x3F) << 6) | (artist_sortname[i + 3] & 0x3F);
                    byte_count = 4;
                } else {
                    // Invalid UTF-8, skip this byte
                    i++;
                    continue;
                }
                
                // Check if character is NOT in Latin range (U+0000 to U+024F)
                if (codepoint > 0x024F) {
                    return false; // Found non-Latin character in sortname
                }
                
                i += byte_count;
            }
            
            return true; // artist_name has both Latin and non-Latin, sortname has only Latin
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
#if 0        
        void
        build_single_artist_index() {
            map<unsigned int, set<unsigned int>> artist_artist_credit_map;
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
                            single_artist_credit_ids.push_back(last_artist_id);
                            single_artist_credit_texts.push_back(it);
                        }
                    }
                    
                    artist_names.push_back(artist_name);
                    // artist_credit_ids are true artist aliases.
                    if (artist_credit_id) 
                        artist_artist_credit_map[artist_id].insert(artist_credit_id);
                    
                    last_artist_id = artist_id;
                }
            
                //insert_artist_credit_mapping(artist_artist_credit_map);

                // Clear the PGresult object to free memory
                PQclear(res);
            }
            catch (std::exception& e)
            {
                printf("build artist db exception: %s\n", e.what());
            }
        }
#endif        
        void
        build_artist_index(const char *query, vector<unsigned int> &ids, vector<string> &texts) {
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
               
                res = PQexec(conn, query);
                if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    printf("Query failed: %s\n", PQerrorMessage(conn));
                    PQclear(res);
                    PQfinish(conn);
                    return;
                }

                for (int i = 0; i < PQntuples(res) + 1; i++) {
                    if (i >= PQntuples(res)) 
                        break;

                    ids.push_back(atoi(PQgetvalue(res, i, 0)));
                    texts.push_back(PQgetvalue(res, i, 1));
                }
            
                // Clear the PGresult object to free memory
                PQclear(res);
            }
            catch (std::exception& e)
            {
                printf("build artist db exception: %s\n", e.what());
            }
        }
        
        void build() {
            
            log("load single artist data");
            build_artist_index(fetch_single_artists_query, single_artist_credit_ids, single_artist_credit_texts);

            vector<unsigned int> single_ids, multiple_ids, stupid_ids;
            vector<string>       single_texts, multiple_texts, stupid_texts; 
            
            // TESTING:
            // - Leave out stupid artists for now.

            // Encode the single artists
            for(unsigned int i = 0; i < single_artist_credit_ids.size(); i++) {
                auto ret = encode.encode_string(single_artist_credit_texts[i]);
                if (ret.size() == 0) {
                    auto stupid = encode.encode_string_for_stupid_artists(single_artist_credit_texts[i]);
                    if (stupid.size()) {
                        stupid_ids.push_back(single_artist_credit_ids[i]);
                        stupid_texts.push_back(stupid);
                        continue;
                    }
                }
                single_ids.push_back(single_artist_credit_ids[i]);
                single_texts.push_back(ret);
            }
            vector<unsigned int>().swap(single_artist_credit_ids);
            vector<string>().swap(single_artist_credit_texts);

            // The extra contexts are so that the stringstreams go out of scope ASAP
            {
                FuzzyIndex *single_artist_index = new FuzzyIndex();
                log("build single artist index");
                single_artist_index->build(single_ids, single_texts);
                std::stringstream ss_single;
                {
                    cereal::BinaryOutputArchive oarchive(ss_single);
                    oarchive(*single_artist_index);
                }
                delete single_artist_index;
                vector<unsigned int>().swap(single_ids);
                vector<string>().swap(single_texts);

                log("artist index size: %lu bytes", ss_single.str().length());
                try
                {
                    SQLite::Database    db(db_file, SQLite::OPEN_READWRITE);
                    SQLite::Statement   query(db, insert_blob_query);
                
                    log("save single artist index");
                    query.bind(1, SINGLE_ARTIST_INDEX_ENTITY_ID);
                    query.bind(2, (const char *)ss_single.str().c_str(), (int32_t)ss_single.str().length());
                    query.exec();
                }
                catch (std::exception& e)
                {
                    printf("save single artist index db exception: %s\n", e.what());
                }
            }
      
            {
                std::stringstream ss_stupid;
                if (stupid_ids.size()) {
                    FuzzyIndex *stupid_artist_index = new FuzzyIndex();
                    log("build stupid artist index");
                    stupid_artist_index->build(stupid_ids, stupid_texts);
                    {
                        cereal::BinaryOutputArchive oarchive(ss_stupid);
                        oarchive(*stupid_artist_index);
                    }
                    log("stupid artist index size: %lu bytes", ss_stupid.str().length());
                    delete stupid_artist_index;

                    if (stupid_ids.size()) {
                        try
                        {
                            SQLite::Database    db(db_file, SQLite::OPEN_READWRITE);
                            SQLite::Statement   query(db, insert_blob_query);
                        
                            SQLite::Statement   query2(db, insert_blob_query);
                            log("save stupid artist index");
                            query2.bind(1, STUPID_ARTIST_INDEX_ENTITY_ID);
                            query2.bind(2, (const char *)ss_stupid.str().c_str(), (int32_t)ss_stupid.str().length());
                            query2.exec();
                        }
                        catch (std::exception& e)
                        {
                            printf("save stupid artist index db exception: %s\n", e.what());
                        }
                    }
                    vector<unsigned int>().swap(stupid_ids);
                    vector<string>().swap(stupid_texts);
                }
            }

            // load and process multiple artists
            log("load multiple artist data");
            build_artist_index(fetch_multiple_artists_query, multiple_artist_credit_ids, multiple_artist_credit_texts);

            for(unsigned int i = 0; i < multiple_artist_credit_ids.size(); i++) {
                auto ret = encode.encode_string(multiple_artist_credit_texts[i]);
                if (ret.size() == 0) {
                }
                else
                {
                    multiple_ids.push_back(multiple_artist_credit_ids[i]);
                    multiple_texts.push_back(ret);
                }
            }
            vector<unsigned int>().swap(multiple_artist_credit_ids);
            vector<string>().swap(multiple_artist_credit_texts);

            {
                FuzzyIndex *multiple_artist_index = new FuzzyIndex();
                log("build multiple artist index");
                multiple_artist_index->build(multiple_ids, multiple_texts);

                std::stringstream ss_multiple;
                {
                    cereal::BinaryOutputArchive oarchive(ss_multiple);
                    oarchive(*multiple_artist_index);
                }
                log("multiple artist index size: %lu bytes", ss_multiple.str().length());
                delete multiple_artist_index;
                vector<unsigned int>().swap(multiple_ids);
                vector<string>().swap(multiple_texts);
                
                try
                {
                    SQLite::Database    db(db_file, SQLite::OPEN_READWRITE);
                    SQLite::Statement   query(db, insert_blob_query);
                
                    log("save multiple artist index");
                    query.bind(1, MULTIPLE_ARTIST_INDEX_ENTITY_ID);
                    query.bind(2, (const char *)ss_multiple.str().c_str(), (int32_t)ss_multiple.str().length());
                    query.exec();
                }
                catch (std::exception& e)
                {
                    printf("save multiple artist index db exception: %s\n", e.what());
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

        void load() {
            if (single_artist_index != nullptr)
                delete single_artist_index;
            single_artist_index = new FuzzyIndex();
            bool ret = load_index(SINGLE_ARTIST_INDEX_ENTITY_ID, single_artist_index);
            if (!ret) {
                throw length_error("failed to load single artist index");
                delete single_artist_index;
                single_artist_index = nullptr;
                return;
            }

            if (multiple_artist_index != nullptr)
                delete multiple_artist_index;
            multiple_artist_index = new FuzzyIndex();
            ret = load_index(MULTIPLE_ARTIST_INDEX_ENTITY_ID, multiple_artist_index);
            if (!ret) {
                throw length_error("failed to load multiple artist index");
                delete multiple_artist_index;
                multiple_artist_index = nullptr;
                return;
            }

            if (stupid_artist_index != nullptr)
                delete stupid_artist_index;
            stupid_artist_index = new FuzzyIndex();
            ret = load_index(STUPID_ARTIST_INDEX_ENTITY_ID, stupid_artist_index);
            if (!ret) {
                throw length_error("failed to load stupid artist index");
                delete single_artist_index;
                single_artist_index = nullptr;
                return;
            }
        }
};
