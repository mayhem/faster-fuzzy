#pragma once
#include <string>
#include <chrono>

using namespace std;

#include "defs.hpp"
#include "encode.hpp"
#include "artist_index.hpp"
#include "index_cache.hpp"
#include "search.hpp"
#include <lb_matching_tools/cleaner.hpp>

// TODO: Review alloc/free of results

// Define all states using a macro
#define STATE_LIST \
    STATE_ITEM(state_start, 0) \
    STATE_ITEM(state_artist_name_check, 1) \
    STATE_ITEM(state_artist_search, 2) \
    STATE_ITEM(state_clean_artist_name, 3) \
    STATE_ITEM(state_clean_artist_search, 4) \
    STATE_ITEM(state_select_artist_match, 5) \
    STATE_ITEM(state_stupid_artist_search, 6) \
    STATE_ITEM(state_recording_search, 7) \
    STATE_ITEM(state_select_recording_match, 8) \
    STATE_ITEM(state_has_release_argument, 9) \
    STATE_ITEM(state_release_search, 10) \
    STATE_ITEM(state_lookup_canonical_release, 11) \
    STATE_ITEM(state_evaluate_match, 12) \
    STATE_ITEM(state_fail, 13) \
    STATE_ITEM(state_success_fetch_metadata, 14) \
    STATE_ITEM(state_select_release_match, 15) \
    STATE_ITEM(state_last, 16)

// Define all events using a macro
#define EVENT_LIST \
    EVENT_ITEM(event_start, 0) \
    EVENT_ITEM(event_yes, 1) \
    EVENT_ITEM(event_no, 2) \
    EVENT_ITEM(event_has_matches, 3) \
    EVENT_ITEM(event_no_matches, 4) \
    EVENT_ITEM(event_normal_name, 5) \
    EVENT_ITEM(event_stupid_name, 6) \
    EVENT_ITEM(event_meets_threshold, 7) \
    EVENT_ITEM(event_doesnt_meet_threshold, 8) \
    EVENT_ITEM(event_evaluate_match, 9) \
    EVENT_ITEM(event_not_cleaned, 10) \
    EVENT_ITEM(event_no_matches_not_cleaned, 11) \
    EVENT_ITEM(event_cleaned, 12)

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
    
    { state_artist_name_check,          event_stupid_name,             state_stupid_artist_search },
    { state_artist_name_check,          event_normal_name,             state_artist_search },

    { state_stupid_artist_search,       event_no_matches,              state_fail },
    { state_stupid_artist_search,       event_has_matches,             state_select_artist_match },

    { state_artist_search,              event_no_matches,              state_clean_artist_name },
    { state_artist_search,              event_has_matches,             state_select_artist_match },
    
    { state_clean_artist_name,          event_cleaned,                 state_artist_name_check },
    { state_clean_artist_name,          event_not_cleaned,             state_fail },

    { state_select_artist_match,        event_meets_threshold,         state_recording_search },
    { state_select_artist_match,        event_doesnt_meet_threshold,   state_fail },

    { state_recording_search,           event_has_matches,             state_select_recording_match },
    { state_recording_search,           event_no_matches,              state_select_artist_match },

    { state_select_recording_match,     event_meets_threshold,         state_has_release_argument },
    { state_select_recording_match,     event_doesnt_meet_threshold,   state_select_artist_match },

    { state_has_release_argument,       event_yes,                     state_release_search },
    { state_has_release_argument,       event_no,                      state_lookup_canonical_release },

    { state_release_search,             event_has_matches,             state_evaluate_match },
    { state_release_search,             event_no_matches,              state_select_artist_match },

    { state_select_release_match,       event_meets_threshold,         state_evaluate_match },
    { state_select_release_match,       event_no_matches,              state_select_release_match },

    { state_lookup_canonical_release,   event_has_matches,             state_evaluate_match },
    { state_lookup_canonical_release,   event_no_matches,              state_fail },
    
    { state_evaluate_match,             event_meets_threshold,         state_success_fetch_metadata },
    { state_evaluate_match,             event_doesnt_meet_threshold,   state_select_recording_match }
};

const int num_transitions = sizeof(transitions) / sizeof(transitions[0]);

class MappingSearch {
    string                              artist_credit_name;
    string                              release_name;
    string                              recording_name;

    
    // Array of function pointers for each state
    typedef bool                        (MappingSearch::*StateFunction)();
    StateFunction                       state_functions[state_last];
    EncodeSearchData                    encode;
    string                              index_dir;

    ArtistIndex                        *artist_index;
    IndexCache                         *index_cache;
    SearchFunctions                    *search_functions;
    lb_matching_tools::MetadataCleaner  metadata_cleaner;

    // FSM variables that carry state for the machine
    // current artist credit name represents the cleaned and/or encoded version of the artist credit
    string                              current_artist_credit_name;
    unsigned int                        selected_artist_credit_id;
    unsigned int                        selected_recording_id;
    unsigned int                        selected_release_id;
    int                                 current_state;
    bool                                has_cleaned_artist, artist_name_cleaned;
    ReleaseRecordingIndex              *release_recording_index;
    vector<IndexResult>                *artist_matches, *release_matches, *recording_matches;     
    int                                 artist_match_index, release_match_index, recording_match_index;
    SearchMatch                        *search_match;
    float                               artist_confidence, release_confidence, recording_confidence;

    public:

        MappingSearch(const string &_index_dir, int cache_size) {
            index_dir = _index_dir;
            artist_index = new ArtistIndex(index_dir);
            index_cache = new IndexCache(cache_size);
            search_functions = new SearchFunctions(index_dir, cache_size);

            // not really needed since init does it, but I feel better like this.
            reset_state_variables();
            
            // Initialize state function array
            state_functions[state_artist_name_check] = &MappingSearch::do_artist_name_check;
            state_functions[state_artist_search] =  &MappingSearch::do_artist_search;
            state_functions[state_clean_artist_name] = &MappingSearch::do_clean_artist_name;
            state_functions[state_select_artist_match] = &MappingSearch::do_select_artist_match; 
            state_functions[state_stupid_artist_search] = &MappingSearch::do_stupid_artist_search;
            state_functions[state_recording_search] = &MappingSearch::do_recording_search;
            state_functions[state_select_recording_match] = &MappingSearch::do_select_recording_match; 
            state_functions[state_has_release_argument] = &MappingSearch::do_has_release_argument;
            state_functions[state_release_search] = &MappingSearch::do_release_search;
            state_functions[state_lookup_canonical_release] = &MappingSearch::do_lookup_canonical_release; 
            state_functions[state_evaluate_match] = &MappingSearch::do_evaluate_match;
            state_functions[state_fail] = &MappingSearch::do_fail;
            state_functions[state_success_fetch_metadata] = &MappingSearch::do_success_fetch_metadata;
        }

        ~MappingSearch() {
            delete artist_index;
            delete index_cache;
            delete search_functions;

            // dealloc state variables
            reset_state_variables();
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
                        log("current %-30s event %-25s new %-30s", 
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

        void reset_state_variables() {
            current_state = state_start;
            artist_name_cleaned = false;
            current_artist_credit_name.clear();

            selected_artist_credit_id = 0;
            selected_recording_id = 0;
            selected_release_id = 0;

            // I wanted to use iterators, but they are too inflexible
            artist_match_index = -1;
            release_match_index = -1;
            recording_match_index = -1;

            artist_confidence = 0.0;
            release_confidence = 0.0;
            recording_confidence = 0.0;

            delete artist_matches;
            artist_matches = nullptr;
            delete release_matches;
            release_matches = nullptr;
            delete recording_matches;
            recording_matches = nullptr;

            // don't delete indexes -- the cache owns the objects
            release_recording_index = nullptr;

            delete search_match;
            search_match = nullptr;
        }

        bool do_artist_name_check() {
            // set current_artist_credit_name
            auto encoded_artist_credit_name = encode.encode_string(artist_credit_name); 
            if (encoded_artist_credit_name.size()) {
                current_artist_credit_name = encoded_artist_credit_name;
                return enter_transition(event_normal_name);
            }
            else {
                current_artist_credit_name = encode.encode_string_for_stupid_artists(artist_credit_name); 
                return enter_transition(event_stupid_name);
            }
        }
        
        bool do_artist_search() {
            // define and store results in artist_matches
            if (artist_matches != nullptr)
                delete artist_matches;

            log("ARTIST SEARCH: '%s' (%s)", artist_credit_name.c_str(), current_artist_credit_name.c_str());
            auto start = std::chrono::high_resolution_clock::now();
            artist_matches = artist_index->single_artist_index->search(current_artist_credit_name, artist_threshold, 's');
            auto multiple_artist_matches = artist_index->multiple_artist_index->search(current_artist_credit_name, artist_threshold, 'm');
            artist_matches->insert(artist_matches->end(), multiple_artist_matches->begin(), multiple_artist_matches->end()); 
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            log("Artist search took %ld ms", duration.count()); 
            
            if (artist_matches->size()) {
                sort(artist_matches->begin(), artist_matches->end(), [](const IndexResult& a, const IndexResult& b) {
                    return a.confidence > b.confidence;
                });

                log("    ARTIST RESULTS:");
                for(const auto &result : *artist_matches) {
                    string name = search_functions->get_artist_credit_name(result.id);
                    log("      %.2f %-8u %c %s", result.confidence, result.id, result.source, name.c_str());
                }

                return enter_transition(event_has_matches);
            }
            else {
                delete artist_matches;
                artist_matches = nullptr;
                if (has_cleaned_artist) 
                    return enter_transition(event_no_matches);
                else
                    return enter_transition(event_no_matches_not_cleaned);
            }
        }

        bool do_clean_artist_name() {
            // update current_artist_credit_name
            auto cleaned_artist_credit_name = metadata_cleaner.clean_artist(artist_credit_name); 
            if (cleaned_artist_credit_name != artist_credit_name) {
                current_artist_credit_name = cleaned_artist_credit_name;
                has_cleaned_artist = true;
                return enter_transition(event_cleaned);
            }
            return enter_transition(event_not_cleaned);
        }

        bool do_stupid_artist_search() {
            // define and store results in artist_matches
            // set artist_match_index to -1, not defined
            // TODO: improve thresholding
            if (artist_matches != nullptr)
                delete artist_matches;
            artist_match_index = -1;

            auto start = std::chrono::high_resolution_clock::now();
            artist_matches = artist_index->stupid_artist_index->search(current_artist_credit_name, .7, 's');
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            log("Stupid artist search took %ld ms", duration.count());
            if (artist_matches->size()) {
                return enter_transition(event_has_matches);
            } else {
                delete artist_matches;
                artist_matches = nullptr;
                return enter_transition(event_no_matches);
            }
        }
        
        bool do_select_artist_match() {
            // set artist_match_index, selected_artist_index_id
            // dealloc relrec index, if one exists

            if (artist_match_index < 0)
                artist_match_index = 0;
            else
                artist_match_index++;

            if (artist_match_index < artist_matches->size() && (*artist_matches)[artist_match_index].confidence >= artist_threshold) {
                selected_artist_credit_id = (*artist_matches)[artist_match_index].id;
                log("artist credit id selected: %u", selected_artist_credit_id);

                // Invalidate the current recording matches
                recording_match_index  = -1;
                delete recording_matches;
                recording_matches = nullptr;

                // don't delete the index, its owned by the cache
                release_recording_index = nullptr;

                return enter_transition(event_meets_threshold);
            }

            // no more matches or doesn't meet threshold, same difference
            return enter_transition(event_doesnt_meet_threshold);
        }

        bool do_recording_search() {
            // check for release_recording_index, load if nullptr
            // set recording_matches
            
            if (release_recording_index == nullptr) {
                release_recording_index = search_functions->load_recording_release_index(selected_artist_credit_id);
            }

            delete recording_matches;
            auto start = std::chrono::high_resolution_clock::now();
            recording_matches = search_functions->recording_search(release_recording_index, recording_name); 
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            log("Recording search took %ld ms", duration.count());
            if (recording_matches && recording_matches->size() > 0)
                return enter_transition(event_has_matches);
           
            return enter_transition(event_no_matches);
        }

        bool do_select_recording_match() {
            // set recording_match
            if (recording_match_index < 0)
                recording_match_index = 0;
            else
                recording_match_index++;

            if (recording_match_index < recording_matches->size() && (*recording_matches)[recording_match_index].confidence >= recording_threshold) {
                selected_recording_id = (*recording_matches)[recording_match_index].id;
                log("recording id selected: %u", selected_recording_id);
                return enter_transition(event_meets_threshold);
            }

            // no more matches or doesn't meet threshold, same difference
            return enter_transition(event_doesnt_meet_threshold);
        }
                
        bool do_has_release_argument() {
            if (release_name.size())
                return enter_transition(event_yes);
            else
                return enter_transition(event_no);
        }
        
        bool do_release_search() {
            // check for release_recording_index, load if nullptr
            // set release_matches
            
            if (release_recording_index == nullptr)
                release_recording_index = search_functions->load_recording_release_index(selected_artist_credit_id);

            delete release_matches;
            auto start = std::chrono::high_resolution_clock::now();
            release_matches = search_functions->release_search(release_recording_index, release_name); 
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            log("Release search took %ld ms", duration.count());
            if (release_matches && release_matches->size() > 0) {
                selected_release_id = (*release_matches)[0].id;
                log("release id selected: %u", selected_release_id);
                release_match_index = 0;
                return enter_transition(event_has_matches);
            }
           
            return enter_transition(event_no_matches);
        } 

        bool do_lookup_canonical_release() {

            // we only need to do this once for a search
            if (release_match_index == 0)
                return enter_transition(event_has_matches);

            // set release_match by looking up canonical release given artist and recording
            release_matches = search_functions->get_canonical_release_id(selected_artist_credit_id, selected_recording_id);
            if (release_matches == nullptr)
                return enter_transition(event_no_matches);

            release_match_index = 0;
            log("canonical release id: %u", (*release_matches)[release_match_index].id);

            return enter_transition(event_has_matches);
        }
        
        bool do_evaluate_match() {
            // select the right link between recording and release
            search_match = search_functions->find_match(selected_artist_credit_id,
                                                        release_recording_index, 
                                                        &(*release_matches)[release_match_index],
                                                        &(*recording_matches)[recording_match_index]);
            if (search_match)
                return enter_transition(event_meets_threshold);
            else
                return enter_transition(event_doesnt_meet_threshold);
        }

        bool do_fail() {
            // total rocket science here!
            return false;
        }

        bool do_success_fetch_metadata() {
            bool ret;

            return search_functions->fetch_metadata(search_match);
        }

        SearchMatch *
        search(const string &artist_credit_name_arg, const string &release_name_arg, const string &recording_name_arg) {
            auto start = std::chrono::high_resolution_clock::now();

            artist_credit_name = artist_credit_name_arg;
            release_name = release_name_arg;
            recording_name = recording_name_arg;

            current_state = state_start;
            reset_state_variables();
            log("START '%s' '%s' '%s'", artist_credit_name.c_str(), release_name.c_str(), recording_name.c_str());
            if (!enter_transition(event_start))  {
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                log("Search took %ld ms", duration.count());
                return nullptr;
            }
            
            log("Final state %s", get_state_name(current_state));
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            log("Search took %ld ms", duration.count());
            SearchMatch *temp = search_match;
            search_match = nullptr;
            return temp;
        }
};