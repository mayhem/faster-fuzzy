#pragma once

#include <mutex>
#include <thread>
#include "defs.hpp"
#include "utils.hpp"

using namespace std;

class IndexCache {
    private:
        map<unsigned int, ArtistReleaseRecordingData *> index;
        map<unsigned int, time_t>                       last_accessed;
        mutex                                           mtx;
        thread                                         *cleaner_thread;
        int                                             max_memory_usage; // in MB
        int                                             cleaning_target; // in MB
        bool                                            stop;

    public:

        // Memory usage is specified in MB
        IndexCache(int max_memory_usage_) { 
            stop = false;
            cleaner_thread = nullptr;
            max_memory_usage = max_memory_usage_;
            cleaning_target = (int)(max_memory_usage * .9);
        }
        
        ~IndexCache() {
            if (cleaner_thread) {
                stop = true;
                cleaner_thread->join();
                delete cleaner_thread;
            }
            clear();
        }
        
        void
        clear() {
            mtx.lock();
            for(auto &item : index)
                delete item.second;
            index.clear();
            mtx.unlock();
        }
        
        long get_memory_footprint() {
            std::ifstream status_file("/proc/self/status");
            std::string line;
            while (std::getline(status_file, line)) {
                if (line.rfind("VmRSS:", 0) == 0) {
                    std::string value;
                    std::istringstream iss(line);
                    std::string key, unit;
                    long rss_kb;
                    iss >> key >> rss_kb >> unit;
                    return rss_kb / 1024; // return a value in MB
                }
            }
            assert(false);
        }
        
        void
        trim() {
            // Obvs, finish this
        }
        
        void
        add(unsigned int artist_credit_id, ArtistReleaseRecordingData *data) {
            mtx.lock();
            index[artist_credit_id] = data;
            mtx.unlock();
        }

        ArtistReleaseRecordingData *
        get(unsigned int artist_credit_id) {
            ArtistReleaseRecordingData *data = nullptr;

            mtx.lock();
            auto iter = index.find(artist_credit_id);
            if (iter != index.end()) {
                data = iter->second;
                time_t cur_time = std::chrono::system_clock::to_time_t(chrono::system_clock::now());
                last_accessed[artist_credit_id] = cur_time;
            }
            mtx.unlock();

            return data;
        }
        
        void cache_cleaner() {
            log("cache cleaner thread started");
            
            long baseline = get_memory_footprint();
            log("%luMB available for index cache", max_memory_usage - baseline);

            while(!stop) {
                this_thread::sleep_for(chrono::seconds(30));
                
                long current = get_memory_footprint();
                auto used = current - baseline;
                if (used >= max_memory_usage) 
                    trim();
            }
            log("cache cleaner thread ended");
        }
        
        void start() {
            cleaner_thread = new thread(IndexCache::_start_cache_cleaner, this);
        }
        
        static void _start_cache_cleaner(IndexCache *obj) {
            obj->cache_cleaner();
        }
};