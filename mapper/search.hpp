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
const float artist_threshold = .6;

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

class MappingSearch {
    private:
        string                                   index_dir;
        ArtistIndex                             *artist_index;
        IndexCache                              *index_cache;
        EncodeSearchData                         encode;
        lb_matching_tools::MetadataCleaner       metadata_cleaner;

    public:

        // cache size is specified in MB
        MappingSearch(const string &_index_dir, int cache_size) {
            index_dir = _index_dir;
            artist_index = new ArtistIndex(index_dir);
            index_cache = new IndexCache(cache_size);
        }
        
        ~MappingSearch() {
            delete artist_index;
            delete index_cache;
        }
        
        void
        load() {
            artist_index->load();
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
        fetch_metadata(SearchResult *result) {
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
#if 0
        void
        fetch_artist_credit_map() {
            string db_file = index_dir + string("/mapping.db");
           
            printf("Load artist credit map!\n");
            try {
                SQLite::Database db(db_file);
                
                // Load the entire artist_credit_mapping table
                SQLite::Statement query(db, "SELECT artist_id, artist_credit_id FROM artist_credit_mapping");
                
                while (query.executeStep()) {
                    unsigned int artist_id = query.getColumn(0).getInt();
                    unsigned int artist_credit_id = query.getColumn(1).getInt();
                    
                    artist_credit_map[artist_id].push_back(artist_credit_id);
                }
            }
            catch (std::exception& e) {
                printf("Error fetching artist credit IDs: %s\n", e.what());
            }
            printf("%lu items\n", artist_credit_map.size());
        }
#endif    
        
        SearchResult *
        recording_release_search(unsigned int artist_credit_id, const string &release_name, const string &recording_name) {
            ReleaseRecordingIndex *release_recording_index;
            
            // Add thresholding

            printf("RELEASE/RECORDING SEARCH for artist credit %d\n", artist_credit_id);
            //auto cleaned_release_name = metadata_cleaner.clean_recording(release_name); 

            release_recording_index = index_cache->get(artist_credit_id);
            if (!release_recording_index) {
                RecordingIndex rec_index(index_dir);
                release_recording_index = rec_index.load(artist_credit_id);
                if (release_recording_index == nullptr)
                    return nullptr;
                index_cache->add(artist_credit_id, release_recording_index);
            }
                
            auto recording_name_encoded = encode.encode_string(recording_name); 
            if (recording_name_encoded.size() == 0) {
                printf("    recording name contains no word characters.\n");
                return nullptr;
            }

            IndexResult rel_result;
            if (release_name.size()) {
                auto release_name_encoded = encode.encode_string(release_name); 
                printf("    RELEASE SEARCH\n");
                if (release_name_encoded.size()) {
                    vector<IndexResult> *rel_results = release_recording_index->release_index->search(release_name_encoded, .7);
                    if (rel_results->size()) {
                        // Sort results by confidence in descending order
                        sort(rel_results->begin(), rel_results->end(), [](const IndexResult& a, const IndexResult& b) {
                            return a.confidence > b.confidence;
                        });
                        
                        for(auto &result : *rel_results) {
                            string text = release_recording_index->release_index->get_index_text(result.result_index);
                            printf("      %.2f %-8u %s\n", result.confidence, result.id, text.c_str());
                        }     
                        rel_result = (*rel_results)[0]; // Now guaranteed to be the best match
                    }
                    else    
                        printf("    no release matches, ignoring release.\n");
                    delete rel_results;
                }
                else
                    printf("  warning: release name contains no word characters, ignoring release.\n");
            }

            printf("    RECORDING SEARCH\n");
            IndexResult rec_result;
            vector<IndexResult> *rec_results = release_recording_index->recording_index->search(recording_name_encoded, .7);
            if (rec_results->size()) {
                // TODO: Are results from FuzzyIndex sorted?
                // Sort results by confidence in descending order
                sort(rec_results->begin(), rec_results->end(), [](const IndexResult& a, const IndexResult& b) {
                    return a.confidence > b.confidence;
                });
                
                for(auto &result : *rec_results) {
                    string text = release_recording_index->recording_index->get_index_text(result.result_index);
                    printf("      %.2f %-8u %s\n", result.confidence, result.id, text.c_str());
                }
                rec_result = (*rec_results)[0]; // Now guaranteed to be the best match
                delete rec_results;
            } else {
                printf("      No recording results.\n");
                delete rec_results;
                return nullptr;
            }
         
            if (rel_result.is_valid && rec_result.is_valid) {
                for(const auto& pair : release_recording_index->links) {
                    if (pair.first == rec_result.result_index) {
                        // Use binary search to find matching release_index since vector is sorted by release_index
                        const auto& links_vector = pair.second;
                        auto it = lower_bound(links_vector.begin(), links_vector.end(), rel_result.id,
                                            [](const ReleaseRecordingLink& link, unsigned int target_release_index) {
                                                return link.release_index < target_release_index;
                                            });
                        
                        // Check if we found a match
                        if (it != links_vector.end() && it->release_index == rel_result.id) {
                            float score = (rec_result.confidence + rel_result.confidence) / 2.0;
                            return new SearchResult(artist_credit_id, it->release_id, it->recording_id, score);
                        }
                        break; // Found the recording, no need to continue searching
                    }
                }  
            }
            if (rec_result.is_valid)
                return new SearchResult(artist_credit_id,
                                        release_recording_index->links[rec_result.result_index][0].release_id,
                                        release_recording_index->links[rec_result.result_index][0].recording_id,
                                        rec_result.confidence);

           return nullptr;
        }
       
        
        SearchResult* search(const string &artist_credit_name, const string &release_name, const string &recording_name) {
            SearchResult            output;
            vector<IndexResult>     *res = nullptr, *mres = nullptr;
            map<unsigned int, int>  ac_history;
            
            auto cleaned_artist_credit_name = metadata_cleaner.clean_artist(artist_credit_name); 

            // TODO: Make sure that we don't process an artist_id more than once!
            auto artist_name = encode.encode_string(artist_credit_name); 
            if (artist_name.size()) {
                printf("SINGLE ARTIST SEARCH: '%s' (%s)\n", artist_credit_name.c_str(), artist_name.c_str());
                res = artist_index->single_artist_index->search(artist_name, .7);
            }
            else {
                auto stupid_name = encode.encode_string_for_stupid_artists(artist_credit_name); 
                if (!stupid_name.size())
                    return nullptr;

                printf("STUPID ARTIST SEARCH: '%s' (%s)\n", artist_credit_name.c_str(), stupid_name.c_str());
                res = artist_index->stupid_artist_index->search(stupid_name, .7);
            }
           
            printf("   MULTIPLE ARTIST SEARCH also\n");
            mres = artist_index->multiple_artist_index->search(artist_name, .7);

            // Sort both result sets by confidence in descending order
            sort(res->begin(), res->end(), [](const IndexResult& a, const IndexResult& b) {
                return a.confidence > b.confidence;
            });
            sort(mres->begin(), mres->end(), [](const IndexResult& a, const IndexResult& b) {
                return a.confidence > b.confidence;
            });

            // Display results interleaved by confidence using two pointers
            size_t res_idx = 0, mres_idx = 0;
            string text;
            
            while (res_idx < res->size() || mres_idx < mres->size()) {
                bool use_res = false;
                
                // Skip results below threshold in res array
                while (res_idx < res->size() && (*res)[res_idx].confidence < artist_threshold) {
                    res_idx++;
                }
                
                // Skip results below threshold in mres array  
                while (mres_idx < mres->size() && (*mres)[mres_idx].confidence < artist_threshold) {
                    mres_idx++;
                }
                
                // Determine which result to use next (only considering above-threshold results)
                if (res_idx >= res->size()) {
                    // No more single results, use multiple
                    use_res = false;
                } else if (mres_idx >= mres->size()) {
                    // No more multiple results, use single
                    use_res = true;
                } else {
                    // Both have results, pick the higher confidence
                    use_res = (*res)[res_idx].confidence >= (*mres)[mres_idx].confidence;
                }
                
                if (use_res) {
                    // Display single artist result - id is already an artist_credit_id
                    IndexResult& result = (*res)[res_idx];
                    text = artist_index->single_artist_index->get_index_text(result.result_index);
                    printf("M  %-9d %.2f %-40s %u\n", result.id, result.confidence, text.c_str(), result.id);
                    res_idx++;
                } else {
                    // Display multiple artist result - id is already an artist_credit_id
                    IndexResult& result = (*mres)[mres_idx];
                    text = artist_index->multiple_artist_index->get_index_text(result.result_index);
                    printf("M  %-9d %.2f %-40s %u\n", result.id, result.confidence, text.c_str(), result.id);
                    mres_idx++;
                }
            }
            printf("\n");
            
            // Process results for recording/release search using same interleaved order
            res_idx = 0; 
            mres_idx = 0;
            SearchResult *best_result = nullptr;
            float best_confidence = 0.0;
            
            while (res_idx < res->size() || mres_idx < mres->size()) {
                bool use_res = false;
                
                // Skip results below threshold in res array
                while (res_idx < res->size() && (*res)[res_idx].confidence < artist_threshold) {
                    res_idx++;
                }
                
                // Skip results below threshold in mres array  
                while (mres_idx < mres->size() && (*mres)[mres_idx].confidence < artist_threshold) {
                    mres_idx++;
                }
                
                // Determine which result to process next (only considering above-threshold results)
                if (res_idx >= res->size()) {
                    use_res = false;
                } else if (mres_idx >= mres->size()) {
                    use_res = true;
                } else {
                    use_res = (*res)[res_idx].confidence >= (*mres)[mres_idx].confidence;
                }
                
                if (use_res) {
                    // Process single/stupid artist result
                    // TODO: Cleanup post complex simple artist stuff
                    IndexResult& result = (*res)[res_idx];
                    auto artist_credit_id = result.id;
                    if (ac_history.find(artist_credit_id) == ac_history.end()) {
                        SearchResult *r = recording_release_search(artist_credit_id, release_name, recording_name); 
                        if (r && r->confidence > .7) {
                            // If this is a very high confidence result (> 0.95), return immediately
                            if (r->confidence > 0.95) {
                                if (!fetch_metadata(r)) {
                                    delete r;
                                    if (best_result) delete best_result;
                                    delete res;
                                    delete mres;
                                    throw std::length_error("failed to load metadata from sqlite.");
                                }
                                if (best_result) delete best_result;
                                delete res;
                                delete mres;
                                return r;
                            }
                            // Otherwise, keep track of the best result so far
                            if (r->confidence > best_confidence) {
                                if (best_result) delete best_result;
                                best_result = r;
                                best_confidence = r->confidence;
                            } else {
                                delete r; // This result is not better than what we have
                            }
                        } else if (r) {
                            printf("ERROR: Could not load release recording index for artist_credit_id %d\n", artist_credit_id);
                            delete r; // Clean up unused result
                        }
                        ac_history[artist_credit_id] = 1;
                    }
                    res_idx++;
                } else {
                    // Process multiple artist result
                    IndexResult& result = (*mres)[mres_idx];
                    unsigned int artist_credit_id = result.id;
                    if (ac_history.find(artist_credit_id) == ac_history.end()) {
                        SearchResult *r = recording_release_search(artist_credit_id, release_name, recording_name); 
                        if (r && r->confidence > .7) {
                            // If this is a very high confidence result (> 0.95), return immediately
                            if (r->confidence > 0.95) {
                                if (!fetch_metadata(r)) {
                                    delete r;
                                    if (best_result) delete best_result;
                                    delete res;
                                    delete mres;
                                    throw std::length_error("failed to load metadata from sqlite.");
                                }
                                if (best_result) delete best_result;
                                delete res;
                                delete mres;
                                return r;
                            }
                            // Otherwise, keep track of the best result so far
                            if (r->confidence > best_confidence) {
                                if (best_result) delete best_result;
                                best_result = r;
                                best_confidence = r->confidence;
                            } else {
                                delete r; // This result is not better than what we have
                            }
                        } else if (r) {
                            printf("ERROR: Could not load release recording index for artist_credit_id %d\n", artist_credit_id);
                            delete r; // Clean up unused result
                        }
                        ac_history[artist_credit_id] = 1;
                    }
                    mres_idx++;
                }
            }
            
            // If we found any good result, return the best one
            if (best_result) {
                if (!fetch_metadata(best_result)) {
                    delete best_result;
                    delete res;
                    delete mres;
                    throw std::length_error("failed to load metadata from sqlite.");
                }
                delete res;
                delete mres;
                return best_result;
            }
            printf("No matches found.\n");
            delete res;
            delete mres;
            return nullptr;
        }
};