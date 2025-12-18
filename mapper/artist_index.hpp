#pragma once
#include <stdio.h>
#include <ctime>
#include <set>
#include <map>
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

// Fetch artist names for artist credits with artist_count = 1
const char *fetch_single_artists_query = R"(
WITH acs AS ( 
   SELECT DISTINCT artist_credit_id  
     FROM mapping.canonical_musicbrainz_data_release_support 
    WHERE artist_credit_id > 1
)
  select ac.id as artist_credit_id
       , ac.name as artist_credit_name
       , array_agg(a.sort_name::text ORDER BY acn.position) as artist_credit_sortname
       , array_agg(acn.join_phrase::text ORDER BY acn.position) as artist_credit_join_phrase      
    from artist_credit ac
    join acs 
      on ac.id = acs.artist_credit_id 
    join artist_credit_name acn
      on acn.artist_credit = ac.id
    join artist a
      on acn.artist = a.id
   where artist_count = 1
     and a.id > 1
group by ac.id
order by ac.id
)";

// Fetch artist names for artist credits with artist_count > 1
const char *fetch_multiple_artists_query = R"(
WITH acs AS ( 
   SELECT DISTINCT artist_credit_id  
     FROM mapping.canonical_musicbrainz_data_release_support 
    WHERE artist_credit_id > 1
)
  select ac.id as artist_credit_id
       , ac.name as artist_credit_name
       , array_agg(a.sort_name::text ORDER BY acn.position) as artist_credit_sortname
       , array_agg(acn.join_phrase::text ORDER BY acn.position) as artist_credit_join_phrase      
    from artist_credit ac
    join acs 
      on ac.id = acs.artist_credit_id 
    join artist_credit_name acn
      on acn.artist_credit = ac.id
    join artist a
      on acn.artist = a.id
   where artist_count > 1
     and a.id > 1
group by ac.id
order by ac.id
)";

const char *fetch_artist_aliases_query = R"(
WITH acs AS ( 
   SELECT DISTINCT artist_credit_id  
     FROM mapping.canonical_musicbrainz_data_release_support 
    WHERE artist_credit_id > 1
)
  select ac.id as artist_credit_id
       , aa.name as artist_credit_name     
    from artist_credit ac 
    join acs 
      on ac.id = acs.artist_credit_id 
    join artist_credit_name acn
      on acn.artist_credit = ac.id
    join artist a
      on acn.artist = a.id
    join artist_alias aa
      on aa.artist = a.id      
   where artist_count = 1
     and a.id > 1
)";

const char *insert_blob_query = R"(
    INSERT INTO index_cache (entity_id, index_data) VALUES (?, ?)
             ON CONFLICT(entity_id) DO UPDATE SET index_data=excluded.index_data)";
const char *fetch_blob_query = 
    "SELECT index_data FROM index_cache WHERE entity_id = ?";

#include <iostream>
#include <vector>
#include <string>
#include <algorithm> // For std::remove_if
#include <libpq-fe.h> // The C API header

vector<string> parse_pg_array(const char* array_str) {
    if (!array_str || array_str[0] != '{') {
        return {}; // Return empty vector if not a valid array string
    }

    vector<string> elements;
    string raw(array_str);
    
    // Remove the surrounding braces: { and }
    raw.erase(0, 1);
    if (!raw.empty() && raw.back() == '}') {
        raw.pop_back();
    }

    string current_element;
    bool inside_quotes = false;
    
    for (size_t i = 0; i < raw.length(); ++i) {
        char c = raw[i];

        if (c == '"') {
            // If the next character is also a quote, it's an escaped quote ("")
            if (i + 1 < raw.length() && raw[i+1] == '"') {
                current_element += '"';
                i++; // Skip the next quote
            } else {
                // Toggle quote state for opening/closing quotes, but don't add the quote character
                inside_quotes = !inside_quotes;
            }
        } else if (c == ',' && !inside_quotes) {
            // Element separator outside of quotes
            elements.push_back(current_element);
            current_element.clear();
        } else {
            // Regular character
            current_element += c;
        }
    }

    if (!current_element.empty() || raw.empty()) {
        elements.push_back(current_element);
    }

    return elements;
}

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
       
        struct TempRow {
            unsigned int artist_id, artist_credit_id;
            string       artist_name, artist_credit_name, encoded_artist_credit_name;
        };

        void
        process_artist_batch(const vector<TempRow>& batch_rows, vector<set<unsigned int>>& artist_credit_groups) {
            if (batch_rows.empty()) {
                return;
            }
            
            // Create a set of all artist_credit_ids for this artist
            set<unsigned int> artist_credit_set;
            
            for (const auto& row : batch_rows) {
                artist_credit_set.insert(row.artist_credit_id);
            }
            
            // Add this set to our collection if it has more than one artist_credit
            if (artist_credit_set.size() > 1) {
                artist_credit_groups.push_back(artist_credit_set);
            }
        }

        void
        load_artist_data(const char *query, vector<unsigned int> &ids, vector<string> &texts) {
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

                    unsigned int artist_credit_id = atoi(PQgetvalue(res, i, 0));
                    string artist_credit_name = PQgetvalue(res, i, 1);
                    vector<string> artist_credit_names = parse_pg_array(PQgetvalue(res, i, 2));
                    vector<string> join_phrases = parse_pg_array(PQgetvalue(res, i, 3));

                    ids.push_back(artist_credit_id);
                    texts.push_back(artist_credit_name);

                    string artist_credit_sort_name;
                    for( size_t i = 0; i < artist_credit_names.size(); i++) {
                        string acn, jp;
                        // father forgive me, for I have pythoned too much
                        try { 
                            acn = artist_credit_names[i];
                        }
                        catch (exception& e) {
                            ;
                        }
                        try { 
                            jp = artist_credit_names[i];
                        }
                        catch (exception& e) {
                            ;
                        }
                        artist_credit_sort_name += acn + jp;

                    }
                    
                    if (is_transliterated(artist_credit_name, artist_credit_sort_name)) {
                        ids.push_back(artist_credit_id);
                        texts.push_back(artist_credit_sort_name);
                    }
                }
            
                // Clear the PGresult object to free memory
                PQclear(res);
            }
            catch (exception& e)
            {
                printf("build artist db exception: %s\n", e.what());
            }
        }

        void
        load_artist_aliases(vector<unsigned int> &ids, vector<string> &texts) {
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
               
                res = PQexec(conn, fetch_artist_aliases_query);
                if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    printf("Query failed: %s\n", PQerrorMessage(conn));
                    PQclear(res);
                    PQfinish(conn);
                    return;
                }

                map<unsigned int, set<string>> alias_groups;
                for (int i = 0; i < PQntuples(res); i++) {
                    unsigned int artist_credit_id = atoi(PQgetvalue(res, i, 0));
                    
                    string encoded = encode.encode_string(PQgetvalue(res, i, 1));
                    if (encoded.size())
                        alias_groups[artist_credit_id].insert(encoded);
                }
                
                for (auto &group : alias_groups) {
                    unsigned int artist_credit_id = group.first;
                    const set<string> &aliases = group.second;
                    
                    for( auto &it : aliases) {
                        ids.push_back(artist_credit_id);
                        texts.push_back(it);
                    }
                }
            
                // Clear the PGresult object to free memory
                PQclear(res);
                PQfinish(conn);
            }
            catch (exception& e)
            {
                printf("build artist aliases db exception: %s\n", e.what());
            }
        }
        
        void build() {
            
            log("load single artist data");
            // TODO: THis process creates duplicates
            load_artist_data(fetch_single_artists_query, single_artist_credit_ids, single_artist_credit_texts);
            load_artist_aliases(single_artist_credit_ids, single_artist_credit_texts);
            
            set<pair<unsigned int, string>> unique_artist_data, stupid_artist_data;

            vector<unsigned int> single_ids, multiple_ids, stupid_ids;
            vector<string>       single_texts, multiple_texts, stupid_texts; 
            
            // TESTING:
            // - Leave out stupid artists for now.

            log("encode and unique artist data");
            // Encode the single artists into sets in order to remove dups
            for(unsigned int i = 0; i < single_artist_credit_ids.size(); i++) {
                auto ret = encode.encode_string(single_artist_credit_texts[i]);
                if (ret.size() == 0) {
                    auto stupid = encode.encode_string_for_stupid_artists(single_artist_credit_texts[i]);
                    if (stupid.size()) {
                        stupid_artist_data.insert({ single_artist_credit_ids[i], stupid }); 
                        continue;
                    }
                }
                unique_artist_data.insert({ single_artist_credit_ids[i], ret }); 
            }
            vector<unsigned int>().swap(single_artist_credit_ids);
            vector<string>().swap(single_artist_credit_texts);
            
            // Convert sets back to vectors for insertion into fuzzyindex
            for(auto &it : unique_artist_data) {
                single_ids.push_back(it.first);
                single_texts.push_back(it.second);
            }
            set<pair<unsigned int, string>>().swap(unique_artist_data);

            for(auto &it : stupid_artist_data) {
                stupid_ids.push_back(it.first);
                stupid_texts.push_back(it.second);
            }
            set<pair<unsigned int, string>>().swap(stupid_artist_data);

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
            load_artist_data(fetch_multiple_artists_query, multiple_artist_credit_ids, multiple_artist_credit_texts);

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
                delete stupid_artist_index;
                stupid_artist_index = nullptr;
                return;
            }
        }
};
