#pragma once

#include <unistd.h>
#include <thread>
#include <chrono>

#include "fuzzy_index.hpp"
#include "recording_index.hpp"
#include "index_cache.hpp"
#include "SQLiteCpp.h"

using namespace std;

const char *fetch_pending_artists_query = R"(
    WITH artist_ids AS (
            SELECT DISTINCT mapping.artist_credit_id
              FROM mapping
         LEFT JOIN index_cache
                ON mapping.artist_credit_id = index_cache.entity_id
             WHERE index_cache.entity_id is null
    )
            SELECT artist_credit_id, count(*) as cnt 
              FROM artist_ids 
             WHERE artist_credit_id > 2 
          GROUP BY artist_credit_id order by cnt desc)";

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
    RecordingIndex  ri(index_dir);
    
    th->sstream = new stringstream();
    
    auto index = ri.build_recording_release_indexes(artist_id);
    {
        cereal::BinaryOutputArchive oarchive(*th->sstream);
        oarchive(*index.recording_index);
        oarchive(*index.release_index);
        oarchive(index.links);
    }
    // Note: index.recording_index and index.release_index are deleted by
    // ReleaseRecordingIndex destructor when 'index' goes out of scope
    th->sstream->seekg(ios_base::end);
    th->sstream->seekg(ios_base::beg);
    th->done = true;
}
    
class MBIDMapping {
    private:
        string                  index_dir, db_file; 

    public:

        MBIDMapping(const string &_index_dir) { 
            index_dir = _index_dir;
            db_file = _index_dir + "/mapping.db";
        }
        
        ~MBIDMapping() {
        }
        
        void write_indexes_to_db(SQLite::Database &db, vector<CreatorThread *> &data) {
            
            SQLite::Transaction transaction(db);
            for(auto &entry : data) {
                SQLite::Statement query(db, insert_blob_query);
           
                query.bind(1, entry->artist_id);
                query.bind(2, (const char *)entry->sstream->str().c_str(), (int32_t)entry->sstream->str().length());
                query.exec();
            }
            transaction.commit();
        }
        
        void build_recording_indexes() { 
            vector<unsigned int> artist_ids; 
            try
            {
                log("Load data");
                SQLite::Database    db(db_file, SQLite::OPEN_READWRITE);
                SQLite::Statement   query(db, fetch_pending_artists_query);

                db.exec("PRAGMA journal_mode=WAL;");
                
                while (query.executeStep())
                    artist_ids.push_back(query.getColumn(0));

                log("Build indexes");
                pair<FuzzyIndex *, FuzzyIndex *> indexes;
                vector<CreatorThread *> threads;
                vector<CreatorThread *> data_to_commit;
                unsigned int count = 0;
                unsigned int total_count = artist_ids.size();
                
                auto now = chrono::system_clock::now();
                time_t t0 = std::chrono::system_clock::to_time_t(now);
                while(artist_ids.size() || threads.size()) {
                    for(int i = threads.size() - 1; i >= 0; i--) {
                        if (i < 0)
                            break;
                            
                        CreatorThread *th = threads[i];
                        if (th->done) {
                            th->th->join();
                            th->th = nullptr;
                            data_to_commit.push_back(th);
                            threads.erase(threads.begin()+i);

                            if (data_to_commit.size() >= NUM_ROWS_PER_COMMIT) {
                                write_indexes_to_db(db, data_to_commit);
                                for(auto &entry : data_to_commit) {
                                    delete entry->sstream;
                                    delete entry;
                                }
                                data_to_commit.clear();
                            }
                            break;
                        }
                    }
                    
                    while (artist_ids.size() && threads.size() < MAX_THREADS) {
                        CreatorThread *newthread = new CreatorThread();
                        unsigned int artist_id = artist_ids[0];
                        artist_ids.erase(artist_ids.begin());
                        newthread->done = false;
                        newthread->artist_id = artist_id;
                        newthread->th = new thread(thread_build_index, index_dir, newthread, artist_id); 
                        threads.push_back(newthread);
                        count++;
                        if ((count % 10) == 0) {
                            auto nower = chrono::system_clock::now();
                            time_t t1 = std::chrono::system_clock::to_time_t(nower);
                            printf("%d%% complete %.1f items/s (%d/%u)     \r", (int)(count * 100/total_count), (float)count/(t1-t0), count, total_count);
                            fflush(stderr);
                        }
                    }    
                }
                write_indexes_to_db(db, data_to_commit);
                for(auto &entry : data_to_commit) {
                    delete entry->sstream;
                    delete entry;
                }                                
                data_to_commit.clear();

                log("indexed %lu rows                             ", count);
            }
            catch (std::exception& e)
            {
                printf("db exception: %s\n", e.what());
            }
            
        }
};
