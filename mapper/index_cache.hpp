#pragma once

#include <mutex>
#include "defs.hpp"

using namespace std;

class IndexCache {
    private:
        map<unsigned int, ArtistReleaseRecordingData *> index;
        map<unsigned int, time_t>                       last_accessed;
        mutex                                           mtx;

    public:

        IndexCache() { 
        }
        
        ~IndexCache() {
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
                    return rss_kb * 1024; // return a value in bytes
                }
            }
            assert(false);
        }
        
        void
        trim(unsigned int size) {

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
};