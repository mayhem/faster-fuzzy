#include <stdio.h>
#include <sstream>

#include "SQLiteCpp.h"

#include "artist_index.hpp"
#include "recording_index.hpp"
#include "mbid_mapping.hpp"
#include "index_cache.hpp"
#include "utils.hpp"
#include "encode.hpp"
#include "levenshtein.hpp"

// TODO: Implement DB connection re-use

const char *fetch_metadata_query_0 = 
    "  WITH input(ind, artist_credit_id, release_id, recording_id) AS ( "
    "       VALUES ";
const char *fetch_metadata_query_1 = 
    "  )"
    "  SELECT ind, artist_mbids, artist_credit_name, release_mbid, release_name, recording_mbid, recording_name "
    "    FROM mapping "
    "    JOIN input "
    "      ON input.artist_credit_id = mapping.artist_credit_id AND "
    "         input.release_id = mapping.release_id AND "
    "         input.recording_id = mapping.recording_id";

class MappingSearch {
    private:
        string              index_dir;
        ArtistIndex        *artist_index;
        IndexCache         *index_cache;
        EncodeSearchData    encode;

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
        fetch_metadata(vector<SearchResult> &results) {
            string db_file = index_dir + string("/mapping.db");
           
            string query = string(fetch_metadata_query_0);
            string values;
            for (size_t i = 0; i < results.size(); i++) {
                if (i > 0) {
                    query += string(", ");
                    values += string(", ");
                } 
                query += string("(?, ?, ?, ?)");
                values += string("(");
                values += to_string(results[i].artist_credit_id);
                values += string(",");
                values += to_string(results[i].release_id);
                values += string(",");
                values += to_string(results[i].recording_id);
                values += string(")\n");
            }
            query += string(fetch_metadata_query_1);

            bool found = false;
            try
            {
                SQLite::Database    db(db_file);
                SQLite::Statement   db_query(db, query);
                
                int index = 1;
                for (const auto& result : results) {
                    db_query.bind(index, (index++)-1);
                    db_query.bind(index++, result.artist_credit_id);
                    db_query.bind(index++, result.release_id);
                    db_query.bind(index++, result.recording_id);
                }

                while (db_query.executeStep()) {
                    index = db_query.getColumn(0).getInt();
                    results[index].artist_credit_mbids = split(db_query.getColumn(1).getString());
                    results[index].artist_credit_name = db_query.getColumn(2).getString();
                    results[index].release_mbid = db_query.getColumn(3).getString();
                    results[index].release_name = db_query.getColumn(4).getString();
                    results[index].recording_mbid = db_query.getColumn(5).getString();
                    results[index].recording_name = db_query.getColumn(6).getString();
                    found = true;
                }
            }
            catch (std::exception& e)
            {
                printf("build artist db exception: %s\n", e.what());
            }
            
            return found;
        }
        
        SearchResult
        recording_release_search(unsigned int artist_credit_id, const string &release_name, const string &recording_name) {
            ArtistReleaseRecordingData *artist_data;
            SearchResult                no_result;
            IndexResult                 rel_result, rec_result;
            
            // Add thresholding

            artist_data = index_cache->get(artist_credit_id);
            if (!artist_data) {
                RecordingIndex rec_index(index_dir);
                artist_data = rec_index.load(artist_credit_id);
                index_cache->add(artist_credit_id, artist_data);
            }
                
            auto recording_name_encoded = encode.encode_string(recording_name); 
            if (recording_name_encoded.size() == 0) {
                printf("recording name contains no word characters.\n");
                return no_result;
            }

            vector<IndexResult> rec_results = artist_data->recording_index->search(recording_name_encoded, .7);
            if (rec_results.size()) {
                rec_result = rec_results[0];
            } else {
                printf("No recording results.\n");
                return no_result;
            }
            
            //        res = artist_data->release_index->search(release_name_encoded, .7);
            //        if (res.size()) {
            //            IndexResult &result = res[0];
            //            EntityRef &ref = (*artist_data->release_data)[result.id].release_refs[0];
            //            release_id = ref.id;
            //        }
            

            if (release_name.size()) {
                auto release_name_encoded = encode.encode_string(release_name); 
                if (release_name_encoded.size()) {
                    vector<IndexResult> rel_results = artist_data->release_index->search(release_name_encoded, .7);
                    if (rel_results.size()) {
                        IndexResult &result = rel_results[0];
                        EntityRef &ref = (*artist_data->release_data)[result.id].release_refs[0];
                        rel_result.id = ref.id;
                        rel_result.confidence = result.confidence;
                    } else
                        printf("warning: no release matches, ignoring release.\n");
                }
                else
                    printf("warning: release name contains no word characters, ignoring release.\n");
            }
            float score;
            if (release_name.size())
                score = (rec_result.confidence + rel_result.confidence) / 2.0;
            else
                score = rec_result.confidence;
                
            SearchResult out(artist_credit_id, rel_result.id, rec_result.id, score);
            return out;
        }
        
        SearchResult
        search(const string &artist_credit_name, const string &release_name, const string &recording_name) {
            SearchResult         output;
            vector<IndexResult>  res;

            auto artist_name = encode.encode_string(artist_credit_name); 
            if (artist_name.size()) {
                res = artist_index->artist_index->search(artist_name, .7);
            }
            else {
                auto stupid_name = encode.encode_string_for_stupid_artists(artist_credit_name); 
                if (!stupid_name.size())
                    return output;

                res = artist_index->stupid_artist_index->search(artist_name, .7);
            }
            if (!res.size())
                return output;
            
            unsigned int artist_credit_id = res[0].id;

            for(auto &it : res) {
                SearchResult r = recording_release_search(artist_credit_id, release_name, recording_name); 
                if (r.confidence > .7) {
                    vector<SearchResult> v;
                    v.push_back(r);
                    if (!fetch_metadata(v))
                        throw std::length_error("failed to load metadata from sqlite.");

                    return r;
                }
            }
            return output;
        }
};