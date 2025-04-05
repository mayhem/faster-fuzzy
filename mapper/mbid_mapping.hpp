#pragma once

#include <unistd.h>

#include "fuzzy_index.hpp"
#include "SQLiteCpp.h"

using namespace std;

const auto MAX_ENCODED_STRING_LENGTH = 30;
const float ARTIST_CONFIDENCE_THRESHOLD = .45;
const int   NUM_ROWS_PER_COMMIT = 25000;
const int   MAX_THREADS = 8;

const char *fetch_recording_query = 
    
class MBIDMapping {
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
        
        void build_mapping_db() { 

            last_row = None
            vector<IndexData> artist_data, stupid_artist_data;
            
            check_if_db_exists(db_file);
             
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