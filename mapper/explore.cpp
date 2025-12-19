#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <sstream>
#include <algorithm>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "SQLiteCpp.h"
#include "artist_index.hpp"
#include "recording_index.hpp"
#include "fsm.hpp"
#include "encode.hpp"
#include "utils.hpp"

using namespace std;

// Function to read a single character without pressing enter
char getch() {
    struct termios oldt, newt;
    char ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

// Function to get terminal height for pagination
int get_terminal_height() {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        return w.ws_row;
    }
    return 24; // Default fallback if ioctl fails
}

class Explorer {
    private:
        string              index_dir;
        ArtistIndex        *artist_index;
        RecordingIndex     *recording_index;
        MappingSearch      *mapping_search;
        IndexCache         *index_cache;
        EncodeSearchData    encode;

    public:
        Explorer(const string &_index_dir) {
            index_dir = _index_dir;
            artist_index = new ArtistIndex(index_dir);
            recording_index = new RecordingIndex(index_dir);
            index_cache = new IndexCache(25);  // 25MB cache
            mapping_search = new MappingSearch(index_dir, artist_index, index_cache);
        }
        
        ~Explorer() {
            delete mapping_search;
            delete artist_index;
            delete recording_index;
            delete index_cache;
        }
        
        void load() {
            artist_index->load();
        }
        
        string make_comma_sep_string(const vector<string> &str_array) {
            string ret; 
            int index = 0;
            for(auto &it : str_array) {
                if (index > 0)
                    ret += string(",");
                        
                ret += it;
                index += 1;
            }
            return ret;
        }
        
        vector<string> split_comma_separated(const string& input) {
            vector<string> result;
            stringstream ss(input);
            string token;
            
            while (getline(ss, token, ',')) {
                // Trim whitespace
                token.erase(0, token.find_first_not_of(" \t\n\r\f\v"));
                token.erase(token.find_last_not_of(" \t\n\r\f\v") + 1);
                result.push_back(token);
            }
            return result;
        }
        
        void full_search(const string& query) {
            vector<string> parts = split_comma_separated(query);
            
            if (parts.size() != 3) {
                log("Usage: s <artist>, <release>, <recording>");
                log("Examples:");
                log("  s portishead, mezzanine, teardrop");
                log("  s bjork, homogenic, joga");
                log("For artist + recording only (no release), use: rs <artist>, <recording>");
                return;
            }
            
            string artist_name = parts[0];
            string release_name = parts[1];
            string recording_name = parts[2];
            
            log("\nFull search: Artist='%s', Release='%s', Recording='%s'\n", 
                   artist_name.c_str(), release_name.c_str(), recording_name.c_str());
            
            SearchMatch* result = nullptr;
            try {
                result = mapping_search->search(artist_name, release_name, recording_name);
            }
            catch (std::exception& e) {
                log("error: %s", e.what());
            }
            
            if (result) {
                log("Search Result:");
                log("-------------");
                log("Artist Credit: %-8d %s %s", 
                    result->artist_credit_id,
                    result->artist_credit_mbids.empty() ? "N/A" : result->artist_credit_mbids[0].c_str(),
                    result->artist_credit_name.c_str());
                log("Release:       %-8d %s %s", 
                    result->release_id,
                    result->release_mbid.c_str(),
                    result->release_name.c_str());
                log("Recording:     %-8d %s %s", 
                    result->recording_id,
                    result->recording_mbid.c_str(),
                    result->recording_name.c_str());
                
                string mbids = make_comma_sep_string(result->artist_credit_mbids);
                log("\nFormatted output:");
                log("{ \"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\" }", 
                    artist_name.c_str(), release_name.c_str(), recording_name.c_str(),
                    mbids.c_str(), result->release_mbid.c_str(), result->recording_mbid.c_str());
                delete result;
            } else {
                log("No match found.");
            }
            log("");
        }
        
        void recording_search(const string& query) {
            vector<string> parts = split_comma_separated(query);
            
            if (parts.size() != 2) {
                log("Usage: rs <artist>, <recording>");
                log("Examples:");
                log("  rs portishead, teardrop");
                log("  rs bjork, joga");
                return;
            }
            
            string artist_name = parts[0];
            string recording_name = parts[1];
            string release_name = ""; // Empty release name for recording-only search
            
            log("\nRecording search: Artist='%s', Recording='%s'\n", 
                   artist_name.c_str(), recording_name.c_str());
            
            SearchMatch* result = mapping_search->search(artist_name, release_name, recording_name);
            
            if (result) {
                log("Search Result:");
                log("-------------");
                log("Artist Credit: %-8d %s %s", 
                    result->artist_credit_id,
                    result->artist_credit_mbids.empty() ? "N/A" : result->artist_credit_mbids[0].c_str(),
                    result->artist_credit_name.c_str());
                log("Release:       %-8d %s %s", 
                    result->release_id,
                    result->release_mbid.c_str(),
                    result->release_name.c_str());
                log("Recording:     %-8d %s %s", 
                    result->recording_id,
                    result->recording_mbid.c_str(),
                    result->recording_name.c_str());
                
                string mbids = make_comma_sep_string(result->artist_credit_mbids);
                log("\nFormatted output:");
                log("{ \"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\" }", 
                    artist_name.c_str(), result->release_name.c_str(), recording_name.c_str(),
                    mbids.c_str(), result->release_mbid.c_str(), result->recording_mbid.c_str());
                
                delete result;
            } else {
                log("No match found.");
            }
            log("");
        }
        
        void search_artist(const string &query) {
            vector<IndexResult> *res = nullptr;
            
            // Try to encode the string normally first
            auto artist_name = encode.encode_string(query);
            if (artist_name.size()) {
                log("ARTIST SEARCH: '%s' (%s)", query.c_str(), artist_name.c_str());
                res = artist_index->single_artist_index->search(artist_name, 0.5, 's');
            }
            else {
                // Try encoding for "stupid artists" (non-Latin characters, etc.)
                auto stupid_name = encode.encode_string_for_stupid_artists(query);
                if (!stupid_name.size()) {
                    log("Could not encode query: '%s'", query.c_str());
                    return;
                }
                
                log("STUPID ARTIST SEARCH: '%s' (%s)", query.c_str(), stupid_name.c_str());
                res = artist_index->stupid_artist_index->search(stupid_name, 0.5, 's');
            }
            
            if (!res->size()) {
                log("  No results found.");
                delete res;
                return;
            }
            
            log("\nResults:");
            log("%-40s %-10s %-8s", "Name", "Confidence", "ID");
            log("------------------------------------------------------------");
            
            for (auto &result : *res) {
                string text;
                if (artist_name.size()) {
                    text = artist_index->single_artist_index->get_index_text(result.result_index);
                } else {
                    text = artist_index->stupid_artist_index->get_index_text(result.result_index);
                }
                
                // Limit artist name to 40 characters
                string short_name = text.length() > 40 ? text.substr(0, 40) : text;
                
                log("%-40s %-10.2f %-8d", short_name.c_str(), result.confidence, result.id);
            }
            log("");
            delete res;
        }

        void debug_artist_search(const string &encoded_text) {
            log("\nDirect artist index search for: '%s'", encoded_text.c_str());
            log("%-8s %-8s %-40s", "Index", "ID", "Artist Text");
            log("------------------------------------------------------------------------");
            
            bool found_any = false;
            
            // Search single artist index
            if (artist_index->single_artist_index) {
                for (size_t i = 0; i < artist_index->single_artist_index->index_texts.size(); i++) {
                    const string& text = artist_index->single_artist_index->index_texts[i];
                    if (text == encoded_text) {
                        unsigned int id = artist_index->single_artist_index->index_ids[i];
                        string display_text = text.length() > 40 ? text.substr(0, 40) : text;
                        log("%-8zu %-8u %-40s [single]", i, id, display_text.c_str());
                        found_any = true;
                    }
                }
            }
            
            // Search multiple artist index
            if (artist_index->multiple_artist_index) {
                for (size_t i = 0; i < artist_index->multiple_artist_index->index_texts.size(); i++) {
                    const string& text = artist_index->multiple_artist_index->index_texts[i];
                    if (text == encoded_text) {
                        unsigned int id = artist_index->multiple_artist_index->index_ids[i];
                        string display_text = text.length() > 40 ? text.substr(0, 40) : text;
                        log("%-8zu %-8u %-40s [multiple]", i, id, display_text.c_str());
                        found_any = true;
                    }
                }
            }
            
            // Search stupid artist index
            if (artist_index->stupid_artist_index) {
                for (size_t i = 0; i < artist_index->stupid_artist_index->index_texts.size(); i++) {
                    const string& text = artist_index->stupid_artist_index->index_texts[i];
                    if (text == encoded_text) {
                        unsigned int id = artist_index->stupid_artist_index->index_ids[i];
                        string display_text = text.length() > 40 ? text.substr(0, 40) : text;
                        log("%-8zu %-8u %-40s [stupid]", i, id, display_text.c_str());
                        found_any = true;
                    }
                }
            }
            
            if (!found_any) {
                log("No matches found for '%s'", encoded_text.c_str());
            }
            log("");
        }

        void search_multiple_artist(const string &query) {
            vector<IndexResult> *res = nullptr;
            
            // Try to encode the string normally first
            auto artist_name = encode.encode_string(query);
            if (artist_name.size()) {
                log("MULTIPLE ARTIST SEARCH: '%s' (%s)", query.c_str(), artist_name.c_str());
                res = artist_index->multiple_artist_index->search(artist_name, 0.5, 'm');
            }
            if (!res->size()) {
                log("  No results found.");
                delete res;
                return;
            }
            
            log("\nResults:");
            log("%-40s %-10s %-8s", "Name", "Confidence", "Artist Credit");
            log("------------------------------------------------------------------------");
            
            for (auto &result : *res) {
                string text;
                if (artist_name.size()) {
                    text = artist_index->multiple_artist_index->get_index_text(result.result_index);
                } else {
                    text = artist_index->multiple_artist_index->get_index_text(result.result_index);
                }
                string short_name = text.length() > 40 ? text.substr(0, 40) : text;
                log("%-40s %-10.2f %-8d", short_name.c_str(), result.confidence, result.id);
            }
            log("");
            delete res;
        }

        void search_stupid_artist(const string &query) {
            vector<IndexResult> *res = nullptr;
            
            // For stupid artist index, we always use stupid encoding
            auto stupid_name = encode.encode_string_for_stupid_artists(query);
            if (!stupid_name.size()) {
                log("Could not encode query for stupid artists: '%s'", query.c_str());
                return;
            }
            
            log("STUPID ARTIST SEARCH: '%s' (%s)", query.c_str(), stupid_name.c_str());
            res = artist_index->stupid_artist_index->search(stupid_name, 0.5, 's');
            
            if (!res->size()) {
                log("  No results found.");
                delete res;
                return;
            }
            
            log("\nResults:");
            log("%-40s %-10s %-8s", "Name", "Confidence", "Artist Credit");
            log("------------------------------------------------------------------------");
            
            for (auto &result : *res) {
                string text = artist_index->stupid_artist_index->get_index_text(result.result_index);
                string short_name = text.length() > 40 ? text.substr(0, 40) : text;
                log("%-40s %-10.2f %-8d", short_name.c_str(), result.confidence, result.id);
            }
            log("");
            delete res;
        }

        void dump_recordings_for_artist_credit(unsigned int artist_credit_id) {
            try {
                log("\nLoading recordings for artist_credit_id: %u", artist_credit_id);
                
                // Query the database directly to show all recordings for this artist credit
                string db_file = index_dir + string("/mapping.db");
                SQLite::Database db(db_file);
                SQLite::Statement direct_query(db, "SELECT DISTINCT recording_id, recording_name FROM mapping WHERE artist_credit_id = ? ORDER BY recording_name");
                direct_query.bind(1, artist_credit_id);
                
                log("\nRecordings:");
                log("%-8s %s", "Rec ID", "Recording Name");
                log("----------------------------------------------------");
                
                int count = 0;
                while (direct_query.executeStep()) {
                    unsigned int rec_id = direct_query.getColumn(0).getInt();
                    string rec_name = direct_query.getColumn(1).getText();
                    log("%-8u %s", rec_id, rec_name.c_str());
                    count++;
                }
                log("Total recordings: %d", count);
                
                if (count == 0) {
                    log("No recordings found in mapping table for artist_credit_id: %u", artist_credit_id);
                }
                
                log("");
                
            } catch (const std::exception& e) {
                log("Error loading recordings for artist_credit_id %u: %s", artist_credit_id, e.what());
            }
        }
        
        void dump_releases_for_artist_credit(unsigned int artist_credit_id) {
            try {
                log("\nLoading releases for artist_credit_id: %u", artist_credit_id);
                
                // Query the database directly to show all releases for this artist credit
                string db_file = index_dir + string("/mapping.db");
                SQLite::Database db(db_file);
                SQLite::Statement direct_query(db, "SELECT DISTINCT release_id, release_name FROM mapping WHERE artist_credit_id = ? ORDER BY release_name");
                direct_query.bind(1, artist_credit_id);
                
                log("\nReleases:");
                log("%-8s %s", "Rel ID", "Release Name");
                log("----------------------------------------------------");
                
                int count = 0;
                while (direct_query.executeStep()) {
                    unsigned int rel_id = direct_query.getColumn(0).getInt();
                    string rel_name = direct_query.getColumn(1).getText();
                    log("%-8u %s", rel_id, rel_name.c_str());
                    count++;
                }
                log("Total releases: %d", count);
                
                if (count == 0) {
                    log("No releases found in mapping table for artist_credit_id: %u", artist_credit_id);
                }
                
                log("");
                
            } catch (const std::exception& e) {
                log("Error loading releases for artist_credit_id %u: %s", artist_credit_id, e.what());
            }
        }
        
        void dump_index_release_data(unsigned int artist_credit_id) {
            try {
                log("\nLoading recording index for artist_credit_id: %u", artist_credit_id);
                
                // Load the recording index for this artist credit
                ReleaseRecordingIndex *data = recording_index->load(artist_credit_id);
                
                if (!data) {
                    log("Failed to load recording index for artist_credit_id: %u", artist_credit_id);
                    return;
                }
                
                log("\n=== RELEASE DATA ===");
                if (data->release_index && !data->links.empty()) {
                    log("%-8s %-50s %-10s %-8s", "Rel_Idx", "Release Text", "Rel_ID", "Rank");
                    log("---------------------------------------------------------------------------------------------");
                    
                    // Collect unique release entries for sorting
                    struct ReleaseEntry {
                        unsigned int release_index;
                        string release_text;
                        unsigned int release_id;
                        unsigned int rank;
                    };
                    map<unsigned int, ReleaseEntry> unique_releases; // Use map to automatically deduplicate by release_index
                    
                    // Gather all release data from links, deduplicating by release_index
                    for (const auto& pair : data->links) {
                        const auto& links_vector = pair.second;
                        for (const auto& link : links_vector) {
                            // Only add if we haven't seen this release_index before
                            if (unique_releases.find(link.release_index) == unique_releases.end()) {
                                string release_text = "";
                                if (link.release_index < data->release_index->index_texts.size()) {
                                    release_text = data->release_index->index_texts[link.release_index];
                                    if (release_text.length() > 50) {
                                        release_text = release_text.substr(0, 50);
                                    }
                                }
                                
                                unique_releases[link.release_index] = {
                                    link.release_index,
                                    release_text,
                                    link.release_id,
                                    link.rank
                                };
                            }
                        }
                    }
                    
                    // Convert map to vector for sorting
                    vector<ReleaseEntry> release_entries;
                    for (const auto& pair : unique_releases) {
                        release_entries.push_back(pair.second);
                    }
                    
                    // Sort by release text
                    sort(release_entries.begin(), release_entries.end(),
                         [](const ReleaseEntry& a, const ReleaseEntry& b) {
                             return a.release_text < b.release_text;
                         });
                    
                    // Display sorted release data
                    for (const auto& entry : release_entries) {
                        log("%-8u %-50s %-10u %-8u", 
                               entry.release_index,
                               entry.release_text.c_str(),
                               entry.release_id,
                               entry.rank);
                    }
                    
                    log("Total release entries: %lu", release_entries.size());
                } else {
                    log("No release data found.");
                }
                
                log("");
                
                // Clean up
                delete data->recording_index;
                delete data->release_index;
                delete data;
                
            } catch (const std::exception& e) {
                log("Error loading recording index for artist_credit_id %u: %s", artist_credit_id, e.what());
            }
        }
        
        void dump_index_recording_data(unsigned int artist_credit_id) {
            try {
                log("\nLoading recording index for artist_credit_id: %u", artist_credit_id);
                
                // Load the recording index for this artist credit
                ReleaseRecordingIndex* data = recording_index->load(artist_credit_id);
                if (!data) {
                    log("Failed to load recording index for artist_credit_id: %u", artist_credit_id);
                    return;
                }
                
                log("\n=== RECORDING INDEX CONTENTS ===");
                if (data->recording_index) {
                    log("Recording Index:");
                    log("%-8s %s", "Rec ID", "Recording Text");
                    log("----------------------------------------------------");
                    
                    // Collect all recording entries and sort by text
                    vector<pair<unsigned int, string>> recordings;
                    for (size_t i = 0; i < data->recording_index->index_ids.size(); i++) {
                        unsigned int id = data->recording_index->index_ids[i];
                        string text = data->recording_index->index_texts[i];
                        recordings.push_back(make_pair(id, text));
                    }
                    
                    // Sort by text value (second element of pair)
                    sort(recordings.begin(), recordings.end(), 
                         [](const pair<unsigned int, string>& a, const pair<unsigned int, string>& b) {
                             return a.second < b.second;
                         });
                    
                    // Print sorted entries
                    for (const auto& recording : recordings) {
                        log("%-8u %s", recording.first, recording.second.c_str());
                    }
                    log("Total recordings in index: %zu", recordings.size());
                } else {
                    log("No recording index found.");
                }
                
                log("");
                
                // Clean up
                delete data->recording_index;
                delete data->release_index;
                delete data;
                
            } catch (const std::exception& e) {
                log("Error loading recording index for artist_credit_id %u: %s", artist_credit_id, e.what());
            }
        }
        
        void dump_links_data(unsigned int artist_credit_id) {
            try {
                log("\nLoading recording index for artist_credit_id: %u", artist_credit_id);
                
                // Load the recording index for this artist credit
                ReleaseRecordingIndex *data = recording_index->load(artist_credit_id);
                
                if (!data) {
                    log("Failed to load recording index for artist_credit_id: %u", artist_credit_id);
                    return;
                }
                
                log("\n=== LINKS TABLE ===");
                if (!data->links.empty()) {
                    log("%-10s %-8s %-21s %-10s %-8s %-8s %-21s", 
                           "Rec_Idx", "Rec_ID", "Recording Name", "Rel_Idx", "Rel_ID", "Rank", "Release Name");
                    log("---------------------------------------------------------------------------------------------------------------");
                    
                    // Get keys sorted by recording_index (not by recording_id)
                    vector<unsigned int> sorted_keys;
                    for (const auto& pair : data->links) {
                        sorted_keys.push_back(pair.first);
                    }
                    sort(sorted_keys.begin(), sorted_keys.end());
                    
                    // First, calculate the actual total number of links
                    size_t total_links = 0;
                    for (unsigned int recording_idx : sorted_keys) {
                        total_links += data->links[recording_idx].size();
                    }
                    
                    size_t lines_printed = 0;
                    const size_t page_size = max(5, get_terminal_height() - 3); // Terminal height minus header/prompt lines
                    
                    for (unsigned int recording_idx : sorted_keys) {
                        const auto& links_vector = data->links[recording_idx];
                        for (const auto& link : links_vector) {
                            // Get release name (truncate to 20 chars)
                            string release_name = "";
                            if (data->release_index && link.release_index < data->release_index->index_texts.size()) {
                                release_name = data->release_index->index_texts[link.release_index];
                                if (release_name.length() > 20) {
                                    release_name = release_name.substr(0, 20);
                                }
                            }
                            
                            // Get recording name (truncate to 20 chars)
                            string recording_name = "";
                            if (data->recording_index && link.recording_index < data->recording_index->index_texts.size()) {
                                recording_name = data->recording_index->index_texts[link.recording_index];
                                if (recording_name.length() > 20) {
                                    recording_name = recording_name.substr(0, 20);
                                }
                            }
                            
                            // Handle sentinel values for release_id and rank
                            const char* release_id_str = (link.release_id == 4294967295) ? "---" : nullptr;
                            const char* rank_str = (link.rank == 4294967295) ? "---" : nullptr;
                            
                            if (release_id_str && rank_str) {
                                log("%-10u %-8u %-21s %-10u %-8s %-8s %-21s", 
                                       link.recording_index, 
                                       link.recording_id,
                                       recording_name.c_str(),
                                       link.release_index, 
                                       release_id_str, 
                                       rank_str, 
                                       release_name.c_str());
                            } else if (release_id_str) {
                                log("%-10u %-8u %-21s %-10u %-8s %-8u %-21s", 
                                       link.recording_index, 
                                       link.recording_id,
                                       recording_name.c_str(),
                                       link.release_index, 
                                       release_id_str, 
                                       link.rank, 
                                       release_name.c_str());
                            } else if (rank_str) {
                                log("%-10u %-8u %-21s %-10u %-8u %-8s %-21s", 
                                       link.recording_index, 
                                       link.recording_id,
                                       recording_name.c_str(),
                                       link.release_index, 
                                       link.release_id, 
                                       rank_str, 
                                       release_name.c_str());
                            } else {
                                log("%-10u %-8u %-21s %-10u %-8u %-8u %-21s", 
                                       link.recording_index, 
                                       link.recording_id,
                                       recording_name.c_str(),
                                       link.release_index, 
                                       link.release_id, 
                                       link.rank, 
                                       release_name.c_str());
                            }
                            lines_printed++;
                            
                            // Check for pagination
                            if (lines_printed % page_size == 0) {
                                printf("<space> to continue, q to quit > ");
                                fflush(stdout);
                                char c = getch();
                                printf("\r"); // Return to beginning of line
                                printf("\033[K"); // Clear the line
                                if (c == 'q' || c == 'Q') {
                                    goto pagination_exit;
                                }
                            }
                        }
                    }
                    pagination_exit:
                    if (lines_printed < total_links) {
                        log("Displayed %zu of %zu total links", lines_printed, total_links);
                    } else {
                        log("Total links: %zu", total_links);
                    }
                } else {
                    log("No links found.");
                }
                
                log("");
                
                // Clean up
                delete data->recording_index;
                delete data->release_index;
                delete data;
                
            } catch (const std::exception& e) {
                log("Error loading recording index for artist_credit_id %u: %s", artist_credit_id, e.what());
            }
        }
        
        string get_line_with_history() {
            char* input = readline("> ");
            
            if (input == nullptr) {
                // EOF (Ctrl-D)
                return "EOF";
            }
            
            string line(input);
            
            // Add to history if not empty
            if (!line.empty()) {
                add_history(input);
            }
            
            free(input);
            return line;
        }
        
        void run_interactive() {
            string input;
            
            log("Music Explorer Interactive Mode");
            log("Index Directory: %s", index_dir.c_str());
            log("\nCommands:");
            log("  a <artist name>              - Search in single artist index");
            log("  m <artist name>              - Search in multiple artist index");
            log("  ! <artist name>              - Search in stupid artist index");
            log("  da <encoded text>            - debug artist index by looking up encoded text");
            log("  drec <artist_credit_id>      - Dump recordings for artist credit from SQLite");
            log("  drel <artist_credit_id>      - Dump releases for artist credit from SQLite");
            log("  rel <artist_credit_id>       - Dump recording index contents and release data");
            log("  rec <artist_credit_id>       - Dump recording index IDs and strings");
            log("  l <artist_credit_id>         - Dump links table for artist credit");
            log("  s <artist>, <release>, <rec> - Full search: artist + release + recording");
            log("  rs <artist>, <recording>     - Recording search: artist + recording (no release)");
            log("  q, .q, \\q, quit, exit        - Quit the program");
            log("\nUse Up/Down arrow keys for command history, Left/Right/Home/End for editing.");
            log("Ctrl+A (beginning), Ctrl+E (end), Ctrl+K (kill to end), Ctrl+U (kill to beginning)\n");
            
            while (true) {
                input = get_line_with_history();
                
                if (input == "EOF") {
                    break;
                }
                
                // Trim whitespace
                input.erase(0, input.find_first_not_of(" \t\n\r\f\v"));
                input.erase(input.find_last_not_of(" \t\n\r\f\v") + 1);
                
                if (input.empty()) {
                    continue;
                }
                
                if (input == "\\q" || input == ".q" || input == "q" || input == "quit" || input == "exit") {
                    break;
                }
                
                // Parse commands
                if (input.substr(0, 2) == "a ") {
                    string artist_query = input.substr(2);
                    if (!artist_query.empty()) {
                        search_artist(artist_query);
                    } else {
                        log("Usage: a <artist name>");
                    }
                } else if (input.substr(0, 2) == "m ") {
                    string artist_query = input.substr(2);
                    if (!artist_query.empty()) {
                        search_multiple_artist(artist_query);
                    } else {
                        log("Usage: m <artist name>");
                    }
                } else if (input.substr(0, 2) == "! ") {
                    string artist_query = input.substr(2);
                    if (!artist_query.empty()) {
                        search_stupid_artist(artist_query);
                    } else {
                        log("Usage: ! <artist name>");
                    }
                } else if (input.substr(0, 3) == "da ") {
                    string encoded_text = input.substr(3);
                    if (!encoded_text.empty()) {
                        debug_artist_search(encoded_text);
                    } else {
                        log("Usage: da <encoded text>");
                        log("Search for encoded text in artist index (all three indexes)");
                    }
                } else if (input.substr(0, 5) == "drec ") {
                    string id_str = input.substr(5);
                    try {
                        unsigned int artist_credit_id = stoul(id_str);
                        dump_recordings_for_artist_credit(artist_credit_id);
                    } catch (const std::exception& e) {
                        log("Invalid artist_credit_id: '%s'. Please enter a valid number.", id_str.c_str());
                    }
                } else if (input.substr(0, 5) == "drel ") {
                    string id_str = input.substr(5);
                    try {
                        unsigned int artist_credit_id = stoul(id_str);
                        dump_releases_for_artist_credit(artist_credit_id);
                    } catch (const std::exception& e) {
                        log("Invalid artist_credit_id: '%s'. Please enter a valid number.", id_str.c_str());
                    }
                } else if (input.substr(0, 4) == "rel ") {
                    string id_str = input.substr(4);
                    try {
                        unsigned int artist_credit_id = stoul(id_str);
                        dump_index_release_data(artist_credit_id);
                    } catch (const std::exception& e) {
                        log("Invalid artist_credit_id: '%s'. Please enter a valid number.", id_str.c_str());
                    }
                } else if (input.substr(0, 4) == "rec ") {
                    string id_str = input.substr(4);
                    try {
                        unsigned int artist_credit_id = stoul(id_str);
                        dump_index_recording_data(artist_credit_id);
                    } catch (const std::exception& e) {
                        log("Invalid artist_credit_id: '%s'. Please enter a valid number.", id_str.c_str());
                    }
                } else if (input.substr(0, 5) == "irec ") {
                    string id_str = input.substr(5);
                    try {
                        unsigned int artist_credit_id = stoul(id_str);
                        dump_index_recording_data(artist_credit_id);
                    } catch (const std::exception& e) {
                        log("Invalid artist_credit_id: '%s'. Please enter a valid number.", id_str.c_str());
                    }
                } else if (input.substr(0, 2) == "l ") {
                    string id_str = input.substr(2);
                    try {
                        unsigned int artist_credit_id = stoul(id_str);
                        dump_links_data(artist_credit_id);
                    } catch (const std::exception& e) {
                        log("Invalid artist_credit_id: '%s'. Please enter a valid number.", id_str.c_str());
                    }
                } else if (input.substr(0, 2) == "s ") {
                    string search_query = input.substr(2);
                    if (!search_query.empty()) {
                        full_search(search_query);
                    } else {
                        log("Usage: s <artist>, <release>, <recording>");
                        log("Examples:");
                        log("  s portishead, mezzanine, teardrop");
                        log("  s björk, homogenic, joga");
                        log("For artist + recording only (no release), use: rs <artist>, <recording>");
                    }
                } else if (input.substr(0, 3) == "rs ") {
                    string search_query = input.substr(3);
                    if (!search_query.empty()) {
                        recording_search(search_query);
                    } else {
                        log("Usage: rs <artist>, <recording>");
                        log("Examples:");
                        log("  rs portishead, teardrop");
                        log("  rs björk, joga");
                    }
                } else {
                    log("Unknown command: '%s'", input.c_str());
                    log("Available commands: a <artist>, rec <id>, rel <id>, irel <id>, j <id>, s <artist>,<release>[,<recording>], rs <artist>,<recording>, q/quit/\\q/.q");
                }
            }
        }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        log("Usage: explore <index_dir>");
        log("  index_dir: Directory containing the mapping database and index files");
        log("\nInteractive music database explorer with artist search and recording/release lookup.");
        return -1;
    }
    
    try {
        string index_dir = argv[1];
        Explorer explorer(index_dir);
        
        log("Loading artist index from: %s", index_dir.c_str());
        explorer.load();
        log("Explorer ready.\n");
        
        explorer.run_interactive();
        
    } catch (const std::exception& e) {
        log("Error: %s", e.what());
        return -1;
    }
    
    return 0;
}
