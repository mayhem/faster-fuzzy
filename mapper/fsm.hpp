#pragma once

const int state_start                         = 0;
const int state_stupid_artist_check           = 1; 
const int state_artist_index_search           = 2;
const int state_clean_artist_name             = 3;
const int state_has_matches                   = 4;
const int state_fetch_alternate_acs           = 5;
const int state_stupid_artist_search          = 6;
const int state_evaluate_artist_results       = 7;
const int state_release_recording_search      = 8;
const int state_fail                          = 9;
const int state_success                       = 10;

const int event_start                         = 0;
const int event_yes                           = 1;
const int event_no                            = 2;
const int event_has_matches                   = 3;
const int event_no_matches                    = 4;
const int event_normal_name                   = 5;
const int event_has_stupid_name               = 6;
const int event_evaluate_matches              = 7;
const int event_adelante                      = 8;


class Transition {
    char initial_state, event, end_state;
};

Transition [] = {
    { state_start,                      event_start,                   state_artist_name_check },

    { state_artist_name_check,          event_has_stupid_name,         state_stupid_artist_search },
    { state_artist_name_check,          event_normal_name,             state_artist_search },

    { state_stupid_artist_search,       event_no_matches,              state_fail },
    { state_stupid_artist_search,       event_has_matches,             state_has_release_argument },

    { state_artist_search,              event_no_matches,              state_clean_artist_name },
    { state_artist_search,              event_no_matches_not_cleaned,  state_clean_artist_name },
    { state_artist_search,              event_has_matches,             state_has_release_arg },

    { state_has_release_arg,            event_yes,                     state_evaluate_artist_results },
    { state_has_release_arg,            event_no,                      state_fetch_alternate_ac_results },

    { state_fetch_alternate_ac_results, event_adelante,                state_evaluate_artist_results },

    { state_evaluate_artist_results,    event_evaluate_matches,        state_release_recording_search },
    { state_evaluate_artist_results,    event_no_matches,              state_clean_artist_name },

    { state_release_recording_search,   event_no_matches,              state_evaluate_artist_results },
    { state_release_recording_search,   event_has_matches,             state_success },
    { state_release_recording_search,   event_no_matches,              state_evaluate_artist_results },

    { state_clean_artist_name,          event_cleaned,                 state_artist_search }
};
