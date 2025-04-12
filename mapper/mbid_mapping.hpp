#pragma once

#include <unistd.h>
#include <thread>

#include "tqdm/tqdm.h"

#include "fuzzy_index.hpp"
#include "recording_index.hpp"
#include "SQLiteCpp.h"

using namespace std;

const float ARTIST_CONFIDENCE_THRESHOLD = .45;
const int   NUM_ROWS_PER_COMMIT = 25000;
const int   MAX_THREADS = 8;

const char *fetch_pending_artists_query = 
    "WITH artist_ids AS ("
    "        SELECT DISTINCT mapping.artist_credit_id"
    "          FROM mapping"
    "     LEFT JOIN index_cache"
    "            ON mapping.artist_credit_id = index_cache.artist_credit_id"
    "         WHERE index_cache.artist_credit_id is null"
    ")"
    "        SELECT artist_credit_id, count(*) as cnt "
    "          FROM artist_ids "
    "      GROUP BY artist_credit_id order by cnt desc";
    

class CreatorThread {
    public:
        unsigned int     artist_id;
        bool             done;
        stringstream    *sstream;
        thread          *th;
        
        CreatorThread() : done(false), th(nullptr), sstream(nullptr)  {
        }
};

void thread_build_index(const string &index_dir, CreatorThread *th, unsigned int artist_id) {
    RecordingIndexes  ri(index_dir);
    
    th->sstream = new stringstream();
    auto indexes = ri.build_recording_release_indexes(artist_id);
    {
        cereal::BinaryOutputArchive oarchive(*th->sstream);
        oarchive(*indexes.first);
        oarchive(*indexes.second);
    }
    delete indexes.first;
    delete indexes.second;
    th->sstream->seekg(ios_base::beg);
    th->done = true;
}
    
class MBIDMapping {
    private:
        string          index_dir, db_file; 

    public:

        MBIDMapping(const string &_index_dir) { 
            index_dir = _index_dir;
            db_file = _index_dir + "/mapping.db";
        }
        
        ~MBIDMapping() {
        }
        
        void write_indexes_to_db(SQLite::Database &db, CreatorThread *th) {

        }
        
        void build_recording_indexes() { 
            vector<unsigned int> artist_ids; 
            try
            {
                SQLite::Database    db(db_file);
                SQLite::Statement   query(db, fetch_pending_artists_query);
                
                while (query.executeStep())
                    artist_ids.push_back(query.getColumn(0));

                pair<FuzzyIndex *, FuzzyIndex *> indexes;
                vector<CreatorThread *> threads;
                for(auto artist_id : tqdm::tqdm(artist_ids)) {

                    for(;;) {
                        bool erased = false;
                        for(int i = threads.size() - 1; i >= 0; i--) {
                            if (i < 0)
                                break;
                                
                            CreatorThread *th = threads[i];
                            if (th->done) {
//                                log("join thread: artist %d", th->artist_id);
                                th->th->join();
                                delete th->sstream;
                                write_indexes_to_db(db, th);
                                threads.erase(threads.begin()+i);
                                delete th;
                                erased = true;
                                break;
                            }
                        }
                        if (erased)
                            continue;
                        
                        if (threads.size() < MAX_THREADS) {
                            CreatorThread *newthread = new CreatorThread();
                            newthread->done = false;
                            newthread->artist_id = artist_id;
//                            log("start thread: artist %d", artist_id);
                            newthread->th = new thread(thread_build_index, index_dir, newthread, artist_id); 
                            threads.push_back(newthread);
                            break;
                        }    
                    }
                }
            }
            catch (std::exception& e)
            {
                printf("db exception: %s\n", e.what());
            }
            
        }
};