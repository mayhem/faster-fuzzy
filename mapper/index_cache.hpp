#pragma once

#include <mutex>
#include <thread>
#include "defs.hpp"
#include "utils.hpp"

using namespace std;

const int SLEEP_DELAY = 30;
const float CLEANING_TARGET_RATIO = 0.9;

class IndexCache {
    private:
        map<unsigned int, ReleaseRecordingIndex *> index;
        map<unsigned int, time_t>                  last_accessed;
        mutex                                      mtx;
        thread                                    *cleaner_thread;
        int                                        max_memory_usage; // in MB
        int                                        cleaning_target; // in MB
        bool                                       stop;

    public:

        // Memory usage is specified in MB
        IndexCache(int max_memory_usage_) { 
            stop = false;
            cleaner_thread = nullptr;
            max_memory_usage = max_memory_usage_;
            cleaning_target = (int)(max_memory_usage * CLEANING_TARGET_RATIO);
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
            last_accessed.clear();
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
            mtx.lock();
            for(; index.size();) {
                vector<std::pair<unsigned int, time_t>> access_times(last_accessed.begin(), last_accessed.end());
                std::sort(access_times.begin(), access_times.end(), [](const auto& a, const auto& b) {
                    return a.second > b.second;
                });
                int items_to_remove = min((size_t)10, index.size());
                for(int i = 0; i < items_to_remove; i++) {
                    unsigned int entity_id = access_times[i].first;
                    delete index[entity_id];
                    index.erase(entity_id);
                    last_accessed.erase(entity_id);
                }
                mtx.unlock();
                
                long current_use = get_memory_footprint();
                if (current_use <= cleaning_target)
                    return;
                mtx.lock();
            }
            mtx.unlock();
        }
        
        // Cache takes ownership of data. Caller must not delete it.
        void
        add(unsigned int artist_credit_id, ReleaseRecordingIndex *data) {
            mtx.lock();
            auto iter = index.find(artist_credit_id);
            if (iter != index.end()) {
                // Already in cache - delete the new one, keep existing
                delete data;
            } else {
                index[artist_credit_id] = data;
                time_t cur_time = std::chrono::system_clock::to_time_t(chrono::system_clock::now());
                last_accessed[artist_credit_id] = cur_time;
            }
            mtx.unlock();
        }

        // Returns pointer to cached data. Caller must NOT delete it - cache owns the memory.
        ReleaseRecordingIndex *
        get(unsigned int artist_credit_id) {
            ReleaseRecordingIndex *data = nullptr;

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
            long baseline = get_memory_footprint();
            log("%luMB available for index cache", max_memory_usage - baseline);

            while(!stop) {
                for(int i = 0; i < SLEEP_DELAY && !stop; i++)
                    this_thread::sleep_for(chrono::seconds(1));
                
                long current = get_memory_footprint();
                auto used = current - baseline;
                if (used >= max_memory_usage) 
                    trim();
            }
        }
        
        void start() {
            cleaner_thread = new thread(IndexCache::_start_cache_cleaner, this);
        }
        
        static void _start_cache_cleaner(IndexCache *obj) {
            obj->cache_cleaner();
        }
};