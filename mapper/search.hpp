#include <stdio.h>
#include <sstream>
#include <set>
#include <map>
#include <algorithm>
#include <memory>

#include "SQLiteCpp.h"

#include "artist_index.hpp"
#include "recording_index.hpp"
#include "indexer_thread.hpp"
#include "index_cache.hpp"
#include "utils.hpp"
#include "encode.hpp"
#include "levenshtein.hpp"
#include <lb_matching_tools/cleaner.hpp>

// TODO: Create dynamic thresholds based on length. Shorter artists will need more checks.
const float artist_threshold = .7;
const float release_threshold = .7;
const float recording_threshold = .7;

const char *fetch_metadata_query = 
    "  SELECT artist_mbids, artist_credit_name, release_mbid, release_name, recording_mbid, recording_name "
    "    FROM mapping "
    "   WHERE mapping.release_id = ? AND "
    "         mapping.recording_id = ?";

const char *fetch_metadata_query_without_release = 
    "  SELECT artist_mbids, artist_credit_name, release_mbid, release_name, recording_mbid, recording_name "
    "    FROM mapping "
    "   WHERE mapping.recording_id = ? "
    "ORDER BY score "
    "   LIMIT 1";

class SearchFunctions {
    private:
        string                              index_dir;
        string                              db_file;
        IndexCache                         *index_cache;  // Shared, not owned
        EncodeSearchData                    encode;
        std::unique_ptr<SQLite::Database>   db;

        // Lazy initialization of DB connection
        SQLite::Database& get_db() {
            if (!db) {
                db = std::make_unique<SQLite::Database>(db_file);
            }
            return *db;
        }

    public:

        // index_cache is shared across threads - caller retains ownership
        SearchFunctions(const string &_index_dir, IndexCache *_index_cache) {
            index_dir = _index_dir;
            db_file = index_dir + string("/mapping.db");
            index_cache = _index_cache;
        }
        
        ~SearchFunctions() {
            // index_cache is shared, don't delete it
        }

        vector<string> split(const std::string& input) {
            vector<std::string> result;
            stringstream        ss(input);
            string              token;

            while (getline(ss, token, ',')) {
                result.push_back(token);
            }

            return result;
        }

        bool
        fetch_metadata(SearchMatch *result) {
            string query;
            
            log("fetch metadata %d %d %d", result->artist_credit_id, result->release_id, result->recording_id);
            
            if (result->release_id)
                query = string(fetch_metadata_query);
            else
                query = string(fetch_metadata_query_without_release);
           
            try
            {
                SQLite::Statement   db_query(get_db(), string(query));
                
                if (result->release_id) {
                    db_query.bind(1, result->release_id);
                    db_query.bind(2, result->recording_id);
                }
                else 
                    db_query.bind(1, result->recording_id);

                while (db_query.executeStep()) {
                    result->artist_credit_mbids = split(db_query.getColumn(0).getString());
                    result->artist_credit_name = db_query.getColumn(1).getString();
                    result->release_mbid = db_query.getColumn(2).getString();
                    result->release_name = db_query.getColumn(3).getString();
                    result->recording_mbid = db_query.getColumn(4).getString();
                    result->recording_name = db_query.getColumn(5).getString();
                    return true;
                }
            }
            catch (std::exception& e)
            {
                printf("fetch metadata db exception: %s\n", e.what());
            }
            
            return false;
        }
       
        // Debug function to fetch artist credit name - can be removed later
        string
        get_artist_credit_name(unsigned int artist_credit_id) {
            try {
                string sql = "SELECT artist_credit_name FROM mapping WHERE artist_credit_id = ? LIMIT 1";
                SQLite::Statement query(get_db(), sql);
                
                query.bind(1, artist_credit_id);

                if (query.executeStep()) {
                    return query.getColumn(0).getString();
                } 
            }
            catch (std::exception& e) {
                printf("get_artist_credit_name db exception: %s\n", e.what());
            }
            return "";
        }

        vector<IndexResult> *
        get_canonical_release_id(unsigned int artist_credit_id, unsigned int recording_id) {
            try {
                string sql = "SELECT release_id FROM mapping WHERE artist_credit_id = ? AND recording_id = ? ORDER BY score LIMIT 1";
                SQLite::Statement query(get_db(), sql);
                
                query.bind(1, artist_credit_id);
                query.bind(2, recording_id);

                if (query.executeStep()) {
                    auto results = new vector<IndexResult>();
                    unsigned int release_id = query.getColumn(0).getUInt();
                    float score = 1.0;
                    results->push_back(IndexResult(release_id, 0, score, 'r'));
                    return results;
                } 
            }
            catch (std::exception& e) {
                printf("get_canonical_release_id db exception: %s\n", e.what());
            }
            return nullptr;
        }

        ReleaseRecordingIndex *
        load_recording_release_index(unsigned int artist_credit_id) {
            
            auto release_recording_index = index_cache->get(artist_credit_id);
            if (!release_recording_index) {
                RecordingIndex rec_index(index_dir);
                release_recording_index = rec_index.load(artist_credit_id, get_db());
                if (release_recording_index == nullptr)
                    return nullptr;
                index_cache->add(artist_credit_id, release_recording_index);
            }

            return release_recording_index;
        }

        vector<IndexResult> *
        release_search(ReleaseRecordingIndex *release_recording_index, 
                       const string          &release_name) {

            // Improve thresholding
            log("    RELEASE SEARCH");
            auto release_name_encoded = encode.encode_string(release_name); 
            if (release_name_encoded.size() == 0) {
                log("    release name contains no word characters.");
                return nullptr;
            }

            vector<IndexResult> *rel_results = release_recording_index->release_index->search(release_name_encoded, .7, 'l');
            if (rel_results->size()) {
                // Sort results by confidence in descending order
                sort(rel_results->begin(), rel_results->end(), [](const IndexResult& a, const IndexResult& b) {
                    return a.confidence > b.confidence;
                });
                
                for(auto &result : *rel_results) {
                    string text = release_recording_index->release_index->get_index_text(result.result_index);
                    log("      %.2f %-8u %-8d %s", result.confidence, result.id, result.result_index, text.c_str());
                }     
            }
            else    
                log("    no release matches, ignoring release.");

            return rel_results;
        }

        vector<IndexResult> *
        recording_search(ReleaseRecordingIndex *release_recording_index, 
                         const string          &recording_name) {

            log("    RECORDING SEARCH");
            auto recording_name_encoded = encode.encode_string(recording_name); 
            if (recording_name_encoded.size() == 0) {
                log("    recording name contains no word characters.");
                return nullptr;
            }

            vector<IndexResult> *rec_results = release_recording_index->recording_index->search(recording_name_encoded, .7, 'c');
            if (rec_results->size()) {
                // Sort results by confidence in descending order
                sort(rec_results->begin(), rec_results->end(), [](const IndexResult& a, const IndexResult& b) {
                    return a.confidence > b.confidence;
                });
                
                for(auto &result : *rec_results) {
                    string text = release_recording_index->recording_index->get_index_text(result.result_index);
                    log("      %.2f %-8u %s", result.confidence, result.id, text.c_str());
                }
            } else {
                log("      No recording results.");
            }

            return rec_results;
        }
        
        SearchMatch *
        find_match(unsigned int           artist_credit_id,
                   ReleaseRecordingIndex *release_recording_index, 
                   IndexResult           *rel_result,
                   IndexResult           *rec_result) {

            for(const auto& pair : release_recording_index->links) {
                if (pair.first == rec_result->result_index) {
                    const auto& links_vector = pair.second;
                    
                    // Different matching strategy based on source:
                    // 'r' = canonical release lookup (match by release_id)
                    // 'l' = fuzzy release search (match by release_index)
                    if (rel_result->source == 'r') {
                        // Canonical lookup: use binary search on release_id (sorted)
                        auto it = lower_bound(links_vector.begin(), links_vector.end(), rel_result->id,
                                            [](const ReleaseRecordingLink& link, unsigned int target_release_id) {
                                                return link.release_id < target_release_id;
                                            });
                        
                        if (it != links_vector.end() && it->release_id == rel_result->id) {
                            float score = (rec_result->confidence + rel_result->confidence) / 2.0;
                            return new SearchMatch(artist_credit_id, it->release_id, it->recording_id, score);
                        }
                    } else {
                        // Fuzzy search: linear search by release_index (not sorted by this field)
                        // Not ideal: consider improving this.
                        for (const auto& link : links_vector) {
                            if (link.release_index == rel_result->result_index) {
                                float score = (rec_result->confidence + rel_result->confidence) / 2.0;
                                return new SearchMatch(artist_credit_id, link.release_id, link.recording_id, score);
                            }
                        }
                    }
                    break; // Found the recording, no need to continue searching
                }
            }  
            log("found no link between recording and release");
            return nullptr;
        }
};