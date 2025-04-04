#pragma once
#include "fuzzy_index.hpp"
#include "SQLiteCpp.h"


using namespace std;

const auto MAX_ENCODED_STRING_LENGTH = 30;

const char *fetch_artist_query = 
    " WITH artist_ids AS ("
    "    SELECT DISTINCT mapping.artist_credit_id"
    "      FROM mapping"
    " LEFT JOIN index_cache"
    "        ON mapping.artist_credit_id = index_cache.artist_credit_id"
    "     WHERE index_cache.artist_credit_id is null"
    " )"
    "    SELECT artist_credit_id, count(*) as cnt"
    "      FROM artist_ids"
    "  GROUP BY artist_credit_id order by cnt desc";
    
class IndexBuilder {
    private:
        FuzzyIndex      index;
        string          index_dir, db_file; 

    public:

        IndexBuilder(const string &_index_dir) { 
            index_dir = _index_dir;
            db_file = _index_dir + "/mapping.db";
        }
        
        ~IndexBuilder() {
        }
        
        void build() {

            vector<unsigned int> artist_ids;
             
            try
            {
                SQLite::Database    db(db_file);
                SQLite::Statement   query(db, fetch_artist_query);
                
                while (query.executeStep())
                    artist_ids.push_back(query.getColumn(0));
            }
            catch (std::exception& e)
            {
                printf("db exception: %s\n", e.what());
            }
            
            for(auto artist_id : artist_ids) {
                build_artist_index(artist_id);
            }
        }

        void build_artist_index(unsigned int artist_id) {



        }
};