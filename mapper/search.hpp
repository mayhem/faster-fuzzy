#include <stdio.h>
#include <sstream>
#include <set>
#include <map>
#include <algorithm>

#include "SQLiteCpp.h"

#include "artist_index.hpp"
#include "recording_index.hpp"
#include "mbid_mapping.hpp"
#include "index_cache.hpp"
#include "utils.hpp"
#include "encode.hpp"
#include "levenshtein.hpp"
#include <lb_matching_tools/cleaner.hpp>

// TODO: Implement DB connection re-use
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
        string                        index_dir;
        IndexCache                   *index_cache;
        EncodeSearchData              encode;

    public:

        // cache size is specified in MB
        SearchFunctions(const string &_index_dir, int cache_size) {
            index_dir = _index_dir;
            index_cache = new IndexCache(cache_size);
        }
        
        ~SearchFunctions() {
            delete index_cache;
        }
        
        void
        start() {
            // TODO: Enable this when we start running a server
            //index_cache.start();
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
            string db_file = index_dir + string("/mapping.db");
            string query;
            
            printf("fetch metadata %d %d %d\n", result->artist_credit_id, result->release_id, result->recording_id);
            
            if (result->release_id)
                query = string(fetch_metadata_query);
            else
                query = string(fetch_metadata_query_without_release);
           
            try
            {
                SQLite::Database    db(db_file);
                SQLite::Statement   db_query(db, string(query));
                
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
            string db_file = index_dir + string("/mapping.db");
            try {
                SQLite::Database db(db_file);
                
                string sql = "SELECT artist_credit_name FROM mapping WHERE artist_credit_id = ? LIMIT 1";
                SQLite::Statement query(db, sql);
                
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
            string db_file = index_dir + string("/mapping.db");
            try {
                SQLite::Database db(db_file);
                
                string sql = "SELECT release_id FROM mapping WHERE artist_credit_id = ? AND recording_id = ? ORDER BY score LIMIT 1";
                SQLite::Statement query(db, sql);
                
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
                release_recording_index = rec_index.load(artist_credit_id);
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
            printf("    RELEASE SEARCH\n");
            auto release_name_encoded = encode.encode_string(release_name); 
            if (release_name_encoded.size() == 0) {
                printf("    release name contains no word characters.\n");
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
                    printf("      %.2f %-8u %-8d %s\n", result.confidence, result.id, result.result_index, text.c_str());
                }     
            }
            else    
                printf("    no release matches, ignoring release.\n");

            return rel_results;
        }

        vector<IndexResult> *
        recording_search(ReleaseRecordingIndex *release_recording_index, 
                         const string          &recording_name) {

            printf("    RECORDING SEARCH\n");
            auto recording_name_encoded = encode.encode_string(recording_name); 
            if (recording_name_encoded.size() == 0) {
                printf("    recording name contains no word characters.\n");
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
                    printf("      %.2f %-8u %s\n", result.confidence, result.id, text.c_str());
                }
            } else {
                printf("      No recording results.\n");
            }

            return rec_results;
        }
        
        SearchMatch *
        find_match(unsigned int           artist_credit_id,
                   ReleaseRecordingIndex *release_recording_index, 
                   IndexResult           *rel_result,
                   IndexResult           *rec_result) {

            printf("    FIND_MATCH DEBUG:\n");
            printf("      rel_result: id=%u, result_index=%d, confidence=%.2f\n", 
                   rel_result->id, rel_result->result_index, rel_result->confidence);
            printf("      rec_result: id=%u, result_index=%d, confidence=%.2f\n", 
                   rec_result->id, rec_result->result_index, rec_result->confidence);
            printf("      Looking for rec_result_index=%d with rel_result_id=%u in links\n",
                   rec_result->result_index, rel_result->id);
            printf("      Total link entries: %zu\n", release_recording_index->links.size());

            // Print all links that point to the release in rel_result
            printf("      Links pointing to release (rel_result->id=%u):\n", rel_result->id);
            for(const auto& pair : release_recording_index->links) {
                for(const auto& link : pair.second) {
                    if (link.release_index == rel_result->id) {
                        printf("        rec_index=%d -> (rel_idx=%u, rel_id=%u, rec_id=%u)\n",
                               pair.first, link.release_index, link.release_id, link.recording_id);
                    }
                }
            }

            // Print all links for the recording in rec_result
            printf("      Links for recording (rec_result->result_index=%d):\n", rec_result->result_index);
            for(const auto& pair : release_recording_index->links) {
                if (pair.first == rec_result->result_index) {
                    for(const auto& link : pair.second) {
                        printf("        rec_index=%d -> (rel_idx=%u, rel_id=%u, rec_id=%u)\n",
                               pair.first, link.release_index, link.release_id, link.recording_id);
                    }
                    break;
                }
            }

            
            for(const auto& pair : release_recording_index->links) {
                if (pair.first == rec_result->result_index) {
                    // Use binary search to find matching release_index since vector is sorted by release_index
                    const auto& links_vector = pair.second;
                    auto it = lower_bound(links_vector.begin(), links_vector.end(), rel_result->result_index,
                                        [](const ReleaseRecordingLink& link, unsigned int target_release_index) {
                                            return link.release_index < target_release_index;
                                        });
                    
                    // Check if we found a match
                    if (it != links_vector.end() && it->release_index == rel_result->result_index) {
                        float score = (rec_result->confidence + rel_result->confidence) / 2.0;
                        return new SearchMatch(artist_credit_id, it->release_id, it->recording_id, score);
                    }
                    break; // Found the recording, no need to continue searching
                }
            }  
            printf("found no link between recording and release\n");
            return nullptr;
        }
};