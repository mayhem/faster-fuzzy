#pragma once
#include <string>

using namespace std;

#include "defs.hpp"
#include "encode.hpp"
#include "artist_index.hpp"
#include "index_cache.hpp"
#include "search.hpp"
#include <lb_matching_tools/cleaner.hpp>

// Define all states using a macro
#define STATE_LIST \
    STATE_ITEM(state_start, 0) \
    STATE_ITEM(state_artist_name_check, 1) \
    STATE_ITEM(state_artist_search, 2) \
    STATE_ITEM(state_clean_artist_name, 3) \
    STATE_ITEM(state_fetch_alternate_acs, 4) \
    STATE_ITEM(state_stupid_artist_search, 5) \
    STATE_ITEM(state_evaluate_artist_matches, 6) \
    STATE_ITEM(state_release_recording_search, 7) \
    STATE_ITEM(state_fail, 8) \
    STATE_ITEM(state_fetch_metadata, 9) \
    STATE_ITEM(state_has_release_argument, 10) \
    STATE_ITEM(state_last, 11)

// Define all events using a macro
#define EVENT_LIST \
    EVENT_ITEM(event_start, 0) \
    EVENT_ITEM(event_yes, 1) \
    EVENT_ITEM(event_no, 2) \
    EVENT_ITEM(event_has_matches, 3) \
    EVENT_ITEM(event_no_matches, 4) \
    EVENT_ITEM(event_normal_name, 5) \
    EVENT_ITEM(event_has_stupid_name, 6) \
    EVENT_ITEM(event_evaluate_match, 7) \
    EVENT_ITEM(event_adelante, 8) \
    EVENT_ITEM(event_no_matches_not_cleaned, 9) \
    EVENT_ITEM(event_cleaned, 10)

// Generate the constants
#define STATE_ITEM(name, value) const int name = value;
STATE_LIST
#undef STATE_ITEM

#define EVENT_ITEM(name, value) const int name = value;
EVENT_LIST
#undef EVENT_ITEM

// Generate the string conversion functions
const char* get_state_name(int state) {
    switch(state) {
        #define STATE_ITEM(name, value) case value: return #name;
        STATE_LIST
        #undef STATE_ITEM
        default: return "unknown_state";
    }
}

const char* get_event_name(int event) {
    switch(event) {
        #define EVENT_ITEM(name, value) case value: return #name;
        EVENT_LIST
        #undef EVENT_ITEM
        default: return "unknown_event";
    }
}


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
    { state_has_release_argument,       event_no,                      state_evaluate_artist_matches },
//    { state_has_release_argument,       event_no,                      state_fetch_alternate_acs },

    { state_fetch_alternate_acs,        event_adelante,                state_evaluate_artist_matches },

    { state_evaluate_artist_matches,    event_evaluate_match,        state_release_recording_search },
    { state_evaluate_artist_matches,    event_no_matches,              state_clean_artist_name },

    { state_release_recording_search,   event_no_matches,              state_evaluate_artist_matches },
    { state_release_recording_search,   event_has_matches,             state_fetch_metadata },

    { state_clean_artist_name,          event_cleaned,                 state_artist_search }
};

const int num_transitions = sizeof(transitions) / sizeof(transitions[0]);

class MappingSearch {
    string                              artist_credit_name, encoded_artist_credit_name;
    string                              stupid_artist_name;
    string                              release_name;
    string                              recording_name;

    int                                 current_state;
    bool                                has_cleaned_artist;
    
    // Array of function pointers for each state
    typedef bool                        (MappingSearch::*StateFunction)();
    StateFunction                       state_functions[state_last];
    EncodeSearchData                    encode;
    string                              index_dir;

    ArtistIndex                        *artist_index;
    IndexCache                         *index_cache;
    SearchFunctions                    *search_functions;
    vector<IndexResult>                *artist_matches;     
    IndexResult                        *current_artist_match;
    SearchMatch                        *current_relrec_match;
    lb_matching_tools::MetadataCleaner  metadata_cleaner;

    public:

        MappingSearch(const string &_index_dir, int cache_size) {
            index_dir = _index_dir;
            artist_index = new ArtistIndex(index_dir);
            index_cache = new IndexCache(cache_size);
            search_functions = new SearchFunctions(index_dir, cache_size);
            
            artist_matches = nullptr;
            current_relrec_match = nullptr;

            current_state = state_start;
            
            // Initialize state function array
            state_functions[state_artist_name_check] = &MappingSearch::do_artist_name_check;
            state_functions[state_artist_search] =  &MappingSearch::do_artist_search;
            state_functions[state_clean_artist_name] = &MappingSearch::do_clean_artist_name;
            state_functions[state_fetch_alternate_acs] = nullptr;
            state_functions[state_stupid_artist_search] = &MappingSearch::do_stupid_artist_search;;
            state_functions[state_evaluate_artist_matches] = &MappingSearch::do_evaluate_artist_matches;
            state_functions[state_release_recording_search] = &MappingSearch::do_release_recording_search;
            state_functions[state_fail] = &MappingSearch::do_fail;
            state_functions[state_fetch_metadata] = &MappingSearch::do_fetch_metadata;
            state_functions[state_has_release_argument] = &MappingSearch::do_has_release_argument;
        }

        ~MappingSearch() {
            delete artist_index;
            delete index_cache;
            delete search_functions;
            if (artist_matches != nullptr)
                delete artist_matches;
            if (current_artist_match != nullptr)
                delete current_artist_match;
            if (current_relrec_match != nullptr)
                delete current_relrec_match;
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
                        printf("current %-30s event %-25s new %-30s\n", 
                               get_state_name(old_state), 
                               get_event_name(event),
                               get_state_name(current_state));
                        (this->*state_functions[current_state])();
                    }
                    else {
                        printf("ERROR: No valid function found for %s via %s\n", 
                               get_state_name(current_state), 
                               get_event_name(event));
                        return false;
                    }
                    
                    return true;
                }
            }
            
            printf("ERROR: No valid transition found from %s with %s\n", 
                   get_state_name(current_state), 
                   get_event_name(event));
            return false;
        }
        
        bool do_artist_name_check() {
            encoded_artist_credit_name = encode.encode_string(artist_credit_name); 
            if (encoded_artist_credit_name.size())
                return enter_transition(event_normal_name);
            else {
                stupid_artist_name = encode.encode_string_for_stupid_artists(artist_credit_name); 
                return enter_transition(event_has_stupid_name);
            }
        }
        
        bool do_artist_search() {
            if (artist_matches != nullptr)
                delete artist_matches;

            printf("ARTIST SEARCH: '%s' (%s)\n", artist_credit_name.c_str(), encoded_artist_credit_name.c_str());
            artist_matches = artist_index->single_artist_index->search(encoded_artist_credit_name, .7, 's');
            auto multiple_artist_matches = artist_index->multiple_artist_index->search(encoded_artist_credit_name, .7, 'm');
            
            artist_matches->insert(artist_matches->end(), multiple_artist_matches->begin(), multiple_artist_matches->end()); 
            
            if (artist_matches->size())
                return enter_transition(event_has_matches);
            else {
                delete artist_matches;
                if (has_cleaned_artist) 
                    return enter_transition(event_no_matches);
                else
                    return enter_transition(event_no_matches_not_cleaned);
            }
        }
        
        bool do_stupid_artist_search() {
            artist_matches = artist_index->stupid_artist_index->search(encoded_artist_credit_name, .7, 's');
            if (artist_matches->size())
                return enter_transition(event_has_matches);
            else {
                delete artist_matches;
                return enter_transition(event_no_matches);
            }
        }
        
        bool do_clean_artist_name() {
            auto cleaned_artist_credit_name = metadata_cleaner.clean_artist(artist_credit_name); 
            if (cleaned_artist_credit_name != artist_credit_name) {
                has_cleaned_artist = false;
                artist_credit_name = cleaned_artist_credit_name;
            }
            return enter_transition(event_cleaned);
        }
                
        bool do_has_release_argument() {
            if (release_name.size())
                return enter_transition(event_yes);
            else
                return enter_transition(event_no);
        }
        
        bool do_evaluate_artist_matches() {
            sort(artist_matches->begin(), artist_matches->end(), [](const IndexResult& a, const IndexResult& b) {
                return a.confidence > b.confidence;
            });
            
            current_artist_match = new IndexResult(artist_matches->front());
            artist_matches->erase(artist_matches->begin());
            if (current_artist_match->confidence > artist_threshold)
                return enter_transition(event_evaluate_match);
            return enter_transition(event_no_matches);
        }

        // We need to add a new state, for doing the search and then eval in another state.
        bool do_release_recording_search() {
            SearchMatch *r = search_functions->recording_release_search(current_artist_match->id, release_name, recording_name); 
            if (r && r->confidence >= release_recording_threshold) {
                current_relrec_match = r;
                return enter_transition(event_has_matches);
            }
            if (r != nullptr)
                delete r;

            return enter_transition(event_no_matches);
        } 
        
        bool do_fail() {
            // total rocket science here!
            return false;
        }
        
        bool do_fetch_metadata() {
            return search_functions->fetch_metadata(current_relrec_match);
        }

        SearchMatch *
        search(const string &artist_credit_name_arg, const string &release_name_arg, const string &recording_name_arg) {
            artist_credit_name = artist_credit_name_arg;
            release_name = release_name_arg;
            recording_name = recording_name_arg;

            has_cleaned_artist = false;
            encoded_artist_credit_name.clear();
            stupid_artist_name.clear();
            
            current_state = state_start;
            if (!enter_transition(event_start)) 
                return nullptr;
            
            printf("Final state %s\n", get_state_name(current_state));
            SearchMatch *temp = current_relrec_match;
            current_relrec_match = nullptr;
            return temp;
        }
};