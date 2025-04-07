#pragma once
#include <stdio.h>
#include "SQLiteCpp.h"
#include "fuzzy_index.hpp"
#include "encode.hpp"

using namespace std;

const char *fetch_artists_query = 
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
                SQLite::Statement   query(db, fetch_artists_query);
                
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
            vector<string>    output_texts, output_rems, stupid_texts, stupid_rems;
                        
            encode.encode_index_data(index_ids, index_texts, output_ids, output_texts, output_rems,
                                                             stupid_ids, stupid_texts, stupid_rems);
            {
                FuzzyIndex index;
                printf("%lu, %lu", output_ids.size(), output_texts.size());
                index.build(output_ids, output_texts);
                // TODO: Write index to DB
            }
            {
                FuzzyIndex index;
                printf("%lu, %lu", stupid_ids.size(), stupid_texts.size());
                index.build(stupid_ids, stupid_texts);
                // TODO: Write index to DB
            }
        }
};
