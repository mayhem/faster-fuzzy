#include <stdio.h>
#include <sstream>
#include <set>
#include <map>

#include "SQLiteCpp.h"

#include "artist_index.hpp"
#include "recording_index.hpp"
#include "mbid_mapping.hpp"
#include "index_cache.hpp"
#include "utils.hpp"
#include "encode.hpp"
#include "levenshtein.hpp"

// TODO: Implement DB connection re-use
// TODO: Create dynamic thresholds based on length. Shorter artists will need more checks.
const float artist_threshold = .6;

const char *fetch_metadata_query = 
    "  SELECT artist_mbids, artist_credit_name, release_mbid, release_name, recording_mbid, recording_name "
    "    FROM mapping "
    "   WHERE mapping.artist_credit_id = ? AND "
    "         mapping.release_id = ? AND "
    "         mapping.recording_id = ?";

const char *fetch_metadata_query_without_release = 
    "  SELECT artist_mbids, artist_credit_name, release_mbid, release_name, recording_mbid, recording_name "
    "    FROM mapping "
    "   WHERE mapping.artist_credit_id = ? AND "
    "         mapping.recording_id = ? "
    "ORDER BY score "
    "   LIMIT 1";

class MappingSearch {
    private:
        string                                   index_dir;
        ArtistIndex                             *artist_index;
        IndexCache                              *index_cache;
        EncodeSearchData                         encode;
        map<unsigned int, vector<unsigned int>>  artist_credit_map;

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
        fetch_metadata(SearchResult &result) {
            string db_file = index_dir + string("/mapping.db");
            string query;
            
            printf("fetch metadata %d %d %d\n", result.artist_credit_id, result.release_id, result.recording_id);
            
            if (result.release_id)
                query = string(fetch_metadata_query);
            else
                query = string(fetch_metadata_query_without_release);
           
            try
            {
                SQLite::Database    db(db_file);
                SQLite::Statement   db_query(db, string(query));
                
                db_query.bind(1, result.artist_credit_id);
                if (result.release_id) {
                    db_query.bind(2, result.release_id);
                    db_query.bind(3, result.recording_id);
                }
                else 
                    db_query.bind(2, result.recording_id);

                while (db_query.executeStep()) {
                    result.artist_credit_mbids = split(db_query.getColumn(0).getString());
                    result.artist_credit_name = db_query.getColumn(1).getString();
                    result.release_mbid = db_query.getColumn(2).getString();
                    result.release_name = db_query.getColumn(3).getString();
                    result.recording_mbid = db_query.getColumn(4).getString();
                    result.recording_name = db_query.getColumn(5).getString();
                    return true;
                }
            }
            catch (std::exception& e)
            {
                printf("fetch metadata db exception: %s\n", e.what());
            }
            
            return false;
        }

        void
        fetch_artist_credit_map() {
            string db_file = index_dir + string("/mapping.db");
            
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
        }
        
        SearchResult
        recording_release_search(unsigned int artist_credit_id, const string &release_name, const string &recording_name) {
            ArtistReleaseRecordingData *artist_data;
            SearchResult                no_result;
            IndexResult                 rel_result = { 0, 0, 0.0 };
            IndexResult                 rec_result = { 0, 0, 0.0 };
            
            // Add thresholding
            printf("  RECORDING RELEASE SEARCH for artist_credit_id %u\n", artist_credit_id);

            artist_data = index_cache->get(artist_credit_id);
            if (!artist_data) {
                RecordingIndex rec_index(index_dir);
                artist_data = rec_index.load(artist_credit_id);
                index_cache->add(artist_credit_id, artist_data);
            }
                
            auto recording_name_encoded = encode.encode_string(recording_name); 
            if (recording_name_encoded.size() == 0) {
                printf("    recording name contains no word characters.\n");
                return no_result;
            }

            if (release_name.size()) {
                auto release_name_encoded = encode.encode_string(release_name); 
                printf("    RELEASE SEARCH: '%s' (%s)\n", release_name.c_str(), release_name_encoded.c_str());
                if (release_name_encoded.size()) {
                    vector<IndexResult> rel_results = artist_data->release_index->search(release_name_encoded, .7);
                    if (rel_results.size()) {
                        IndexResult &result = rel_results[0];
                        EntityRef &ref = (*artist_data->release_data)[result.id].release_refs[0];
                        rel_result.id = ref.id;
                        rel_result.confidence = result.confidence;

                        string text = artist_data->release_index->get_index_text(rel_results[0].result_index);
                        printf("      %s %.2f\n", text.c_str(), rel_results[0].confidence);
                    } else
                        printf("      no release matches, ignoring release.\n");
                }
                else
                    printf("    warning: release name contains no word characters, ignoring release.\n");
            }

            printf("    RECORDING SEARCH: '%s' (%s)\n", recording_name.c_str(), recording_name_encoded.c_str());
            vector<IndexResult> rec_results = artist_data->recording_index->search(recording_name_encoded, .7);
            if (rec_results.size()) {
                rec_result = rec_results[0];
                string text = artist_data->recording_index->get_index_text(rec_results[0].result_index);
                printf("      %s %.2f\n", text.c_str(), rec_results[0].confidence);
            } else {
                printf("      No recording results.\n");
                return no_result;
            }
            
            float score;
            if (release_name.size()) {
                score = (rec_result.confidence + rel_result.confidence) / 2.0;
                SearchResult out(artist_credit_id, rel_result.id, rec_result.id, score);
                return out;
            } else {
                score = rec_result.confidence;
                SearchResult out(artist_credit_id, 0, rec_result.id, score);
                return out;
            }
                
        }
       
        
        SearchResult *
        search(const string &artist_credit_name, const string &release_name, const string &recording_name) {
            SearchResult            output;
            vector<IndexResult>     res;
            map<unsigned int, int>  ac_history;
            
            if (artist_credit_map.empty()) {
                printf("load artist credit map\n");
                fetch_artist_credit_map();
            }

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
           
            // TODO: maybe better? If the top hit is not 90% or better, do a multiple artist search and merge the results
            printf("conf %.3f\n", res[0].confidence);
            if (!res.size() || res[0].confidence < .9) {
                printf("MULTIPLE ARTIST SEARCH: '%s' (%s)\n", artist_credit_name.c_str(), artist_name.c_str());
                res = artist_index->multiple_artist_index->search(artist_name, .7);
            }

            if (!res.size()) {
                printf("  no results\n");
                return nullptr;
            }
            else {
                string text;
                for(auto &it : res) {
                    if (it.confidence >= artist_threshold) {
                        if (artist_name.size()) 
                            text = artist_index->single_artist_index->get_index_text(it.result_index);
                        else
                            text = artist_index->stupid_artist_index->get_index_text(it.result_index);
                        printf("  %-9d %.2f %-40s ", it.id, it.confidence, text.c_str());

                        for(auto it : artist_credit_map[it.id]) 
                            printf("%u ", it);
                        printf("\n");
                    }
                }
                printf("\n");
            }
            
            for(auto &it : res) {
                if (it.confidence < artist_threshold) 
                    continue;

                for (auto artist_credit_id : artist_credit_map[it.id]) {
                    if (ac_history.find(artist_credit_id) == ac_history.end()) {
                        SearchResult r = recording_release_search(artist_credit_id, release_name, recording_name); 
                        if (r.confidence > .7) {
                            if (!fetch_metadata(r))
                                throw std::length_error("failed to load metadata from sqlite.");
                            return new SearchResult(r);
                        }
                        ac_history[artist_credit_id] = 1;
                    }
                }
            }
            printf("No matches found.\n");
            return nullptr;
        }
};