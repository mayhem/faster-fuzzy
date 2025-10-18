#pragma once
#include <string>

using namespace std;

#include "defs.hpp"
#include "encode.hpp"
#include "artist_index.hpp"
#include "index_cache.hpp"
#include <lb_matching_tools/cleaner.hpp>

const int state_start                         = 0;
const int state_artist_name_check             = 1; 
const int state_artist_search                 = 2;
const int state_clean_artist_name             = 3;
const int state_fetch_alternate_acs           = 4;
const int state_stupid_artist_search          = 5;
const int state_evaluate_artist_matches       = 6;
const int state_release_recording_search      = 7;
const int state_fail                          = 8;
const int state_success                       = 9;
const int state_has_release_argument          = 10;
const int state_last                          = 11;  // make sure this is always the last state!

const int event_start                         = 0;
const int event_yes                           = 1;
const int event_no                            = 2;
const int event_has_matches                   = 3;
const int event_no_matches                    = 4;
const int event_normal_name                   = 5;
const int event_has_stupid_name               = 6;
const int event_evaluate_matches              = 7;
const int event_adelante                      = 8;
const int event_no_matches_not_cleaned        = 9;
const int event_cleaned                       = 10;


struct Transition {
    char initial_state, event, end_state;
};

static Transition transitions[] = {
    { state_start,                      event_start,                   state_artist_name_check },
    
    { state_artist_name_check,          event_has_stupid_name,         state_stupid_artist_search },
    { state_artist_name_check,          event_normal_name,             state_artist_search },

    { state_stupid_artist_search,       event_no_matches,              state_fail },
    { state_stupid_artist_search,       event_has_matches,             state_has_release_argument },

    { state_artist_search,              event_no_matches,              state_clean_artist_name },
    { state_artist_search,              event_no_matches_not_cleaned,  state_clean_artist_name },
    { state_artist_search,              event_has_matches,             state_has_release_argument },

    { state_has_release_argument,       event_yes,                     state_evaluate_artist_matches },
    { state_has_release_argument,       event_no,                      state_fetch_alternate_acs },

    { state_fetch_alternate_acs,        event_adelante,                state_evaluate_artist_matches },

    { state_evaluate_artist_matches,    event_evaluate_matches,        state_release_recording_search },
    { state_evaluate_artist_matches,    event_no_matches,              state_clean_artist_name },

    { state_release_recording_search,   event_no_matches,              state_evaluate_artist_matches },
    { state_release_recording_search,   event_has_matches,             state_success },
    { state_release_recording_search,   event_no_matches,              state_evaluate_artist_matches },

    { state_clean_artist_name,          event_cleaned,                 state_artist_search }
};

const int num_transitions = sizeof(transitions) / sizeof(transitions[0]);

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

class FSMMappingSearch {
    string                              artist_credit_name, encoded_artist_credit_name;
    string                              stupid_artist_name;
    string                              release_name;
    string                              recording_name;

    int                                 current_state;
    bool                                has_cleaned_artist;
    
    // Array of function pointers for each state
    typedef void                        (FSMMappingSearch::*StateFunction)();
    StateFunction                       state_functions[state_last];
    EncodeSearchData                    encode;
    string                              index_dir;

    ArtistIndex                        *artist_index;
    IndexCache                         *index_cache;
    vector<IndexResult>                *artist_matches;     
    lb_matching_tools::MetadataCleaner  metadata_cleaner;

    public:

        FSMMappingSearch(const string &_index_dir, int cache_size) {
            index_dir = _index_dir;
            artist_index = new ArtistIndex(index_dir);
            index_cache = new IndexCache(cache_size);

            current_state = state_start;
            
            // Initialize state function array
            state_functions[state_artist_name_check] = &FSMMappingSearch::do_state_artist_name_check;
            state_functions[state_artist_search] =  &FSMMappingSearch::do_artist_search;
            state_functions[state_clean_artist_name] = nullptr;
            state_functions[state_fetch_alternate_acs] = nullptr;
            state_functions[state_stupid_artist_search] = &FSMMappingSearch::do_stupid_artist_search;;
            state_functions[state_evaluate_artist_matches] = nullptr;
            state_functions[state_release_recording_search] = nullptr;
            state_functions[state_fail] = nullptr;
            state_functions[state_success] = nullptr;
            state_functions[state_has_release_argument] = nullptr;
        }

        ~FSMMappingSearch() {
            delete artist_index;
            delete index_cache;
            if (artist_matches != nullptr)
                delete artist_matches;
        }

        void
        load() {
            artist_index->load();
            // TODO: Enable this when we start running a server
            //index_cache.start();
        }
        
        bool enter_transition(int event) {
            for (size_t i = 0; i < num_transitions; i++) {
                if (transitions[i].initial_state == current_state && transitions[i].event == event) {
                    int old_state = current_state;
                    current_state = transitions[i].end_state;
                    
                    if (state_functions[current_state] != nullptr) {
                        printf("transition from %d to %d via event %d\n", old_state, current_state, event);
                        (this->*state_functions[current_state])();
                    }
                    else {
                        printf("ERROR: No valid function found state %d with event %d\n", current_state, event);
                        return false;
                    }
                    
                    return true;
                }
            }
            
            printf("ERROR: No valid transition found from state %d with event %d\n", current_state, event);
            return false;
        }
        
        void do_state_artist_name_check() {
            encoded_artist_credit_name = encode.encode_string(artist_credit_name); 
            if (encoded_artist_credit_name.size())
                enter_transition(event_normal_name);
            else {
                stupid_artist_name = encode.encode_string_for_stupid_artists(artist_credit_name); 
                enter_transition(event_has_stupid_name);
            }
        }
        
        void do_artist_search() {
            if (artist_matches != nullptr)
                delete artist_matches;

            printf("ARTIST SEARCH: '%s' (%s)\n", artist_credit_name.c_str(), encoded_artist_credit_name.c_str());
            artist_matches = artist_index->single_artist_index->search(encoded_artist_credit_name, .7, 's');
            auto multiple_artist_matches = artist_index->multiple_artist_index->search(encoded_artist_credit_name, .7, 'm');
            
            artist_matches->insert(artist_matches->end(), multiple_artist_matches->begin(), multiple_artist_matches->end()); 
            
            sort(artist_matches->begin(), artist_matches->end(), [](const IndexResult& a, const IndexResult& b) {
                return a.confidence > b.confidence;
            });
            
            if (artist_matches->size())
                enter_transition(event_has_matches);
            else {
                delete artist_matches;
                if (has_cleaned_artist) 
                    enter_transition(event_no_matches);
                else
                    enter_transition(event_no_matches_not_cleaned);
            }
        }
        
        void do_stupid_artist_search() {
            artist_matches = artist_index->stupid_artist_index->search(encoded_artist_credit_name, .7, 's');
            if (artist_matches->size())
                enter_transition(event_has_matches);
            else {
                delete artist_matches;
                enter_transition(event_no_matches);
            }
        }
        
        void do_clean_artist_name() {
            auto cleaned_artist_credit_name = metadata_cleaner.clean_artist(artist_credit_name); 
            if (cleaned_artist_credit_name != artist_credit_name) {
                has_cleaned_artist = false;
                artist_credit_name = cleaned_artist_credit_name;
            }
            enter_transition(event_cleaned);
        }
                
        void do_state_has_release_argument() {
            if (release_name.size())
                enter_transition(event_yes);
            else
                enter_transition(event_no);
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
        fetch_metadata(SearchMatches *result) {
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
        
        vector<unsigned int>
        fetch_alternate_artist_credits(const vector<unsigned int>& artist_credit_ids) {
            vector<unsigned int> alternates;
            if (artist_credit_ids.empty()) {
                return alternates;
            }
            
            string db_file = index_dir + string("/mapping.db");
            
            try {
                SQLite::Database db(db_file);
                
                string placeholders;
                for (size_t i = 0; i < artist_credit_ids.size(); i++) {
                    if (i > 0) placeholders += ",";
                    placeholders += "?";
                }
                
                string sql = "SELECT DISTINCT alternate_artist_credit_id FROM alternate_artist_credits WHERE artist_credit_id IN (" + placeholders + ")";
                SQLite::Statement query(db, sql);
                
                // Bind all the artist_credit_ids
                for (size_t i = 0; i < artist_credit_ids.size(); i++) {
                    query.bind(i + 1, artist_credit_ids[i]);
                }
                
                while (query.executeStep()) {
                    unsigned int alternate_id = query.getColumn(0).getUInt();
                    alternates.push_back(alternate_id);
                }
            }
            catch (std::exception& e) {
                printf("fetch_alternate_artist_credits db exception: %s\n", e.what());
            }
            
            return alternates;
        }

        SearchMatches *
        search(const string &artist_credit_name_arg, const string &release_name_arg, const string &recording_name_arg) {
            artist_credit_name = artist_credit_name_arg;
            release_name = release_name_arg;
            recording_name = recording_name_arg;

            has_cleaned_artist = false;
            encoded_artist_credit_name.clear();
            stupid_artist_name.clear();

            if (!enter_transition(event_start)) 
                return nullptr;
            
            printf("Final state %d\n", current_state);
            return nullptr;
        }
};