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
#include "search.hpp"
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
        EncodeSearchData    encode;
        map<unsigned int, vector<unsigned int>> artist_credit_map;

    public:
        Explorer(const string &_index_dir) {
            index_dir = _index_dir;
            artist_index = new ArtistIndex(index_dir);
            recording_index = new RecordingIndex(index_dir);
            mapping_search = new MappingSearch(index_dir, 25); // 25MB cache
        }
        
        ~Explorer() {
            delete artist_index;
            delete recording_index;
            delete mapping_search;
        }
        
        void load() {
            artist_index->load();
            mapping_search->load();
        }
       
        map<unsigned int, vector<unsigned int>>
        fetch_artist_credit_ids(const vector<IndexResult> &res) {
            map<unsigned int, vector<unsigned int>> result_map;
            
            // Use the preloaded artist credit map instead of querying database
            for (const auto& result : res) {
                auto it = artist_credit_map.find(result.id);
                if (it != artist_credit_map.end()) {
                    result_map[result.id] = it->second;
                }
            }
            
            return result_map;
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
                printf("Usage: s <artist>, <release>, <recording>\n");
                printf("Examples:\n");
                printf("  s portishead, mezzanine, teardrop\n");
                printf("  s bjork, homogenic, joga\n");
                printf("For artist + recording only (no release), use: rs <artist>, <recording>\n");
                return;
            }
            
            string artist_name = parts[0];
            string release_name = parts[1];
            string recording_name = parts[2];
            
            printf("\nFull search: Artist='%s', Release='%s', Recording='%s'\n\n", 
                   artist_name.c_str(), release_name.c_str(), recording_name.c_str());
            
            SearchResult* result = nullptr;
            try {
                result = mapping_search->search(artist_name, release_name, recording_name);
            }
            catch (std::exception& e) {
                printf("error: %s\n", e.what());
            }
            
            if (result) {
                printf("Search Result:\n");
                printf("-------------\n");
                printf("Artist Credit: %-8d %s %s\n", 
                    result->artist_credit_id,
                    result->artist_credit_mbids.empty() ? "N/A" : result->artist_credit_mbids[0].c_str(),
                    result->artist_credit_name.c_str());
                printf("Release:       %-8d %s %s\n", 
                    result->release_id,
                    result->release_mbid.c_str(),
                    result->release_name.c_str());
                printf("Recording:     %-8d %s %s\n", 
                    result->recording_id,
                    result->recording_mbid.c_str(),
                    result->recording_name.c_str());
                
                string mbids = make_comma_sep_string(result->artist_credit_mbids);
                printf("\nFormatted output:\n");
                printf("{ \"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\" }\n", 
                    artist_name.c_str(), release_name.c_str(), recording_name.c_str(),
                    mbids.c_str(), result->release_mbid.c_str(), result->recording_mbid.c_str());
                
                delete result;
            } else {
                printf("No match found.\n");
            }
            printf("\n");
        }
        
        void recording_search(const string& query) {
            vector<string> parts = split_comma_separated(query);
            
            if (parts.size() != 2) {
                printf("Usage: rs <artist>, <recording>\n");
                printf("Examples:\n");
                printf("  rs portishead, teardrop\n");
                printf("  rs bjork, joga\n");
                return;
            }
            
            string artist_name = parts[0];
            string recording_name = parts[1];
            string release_name = ""; // Empty release name for recording-only search
            
            printf("\nRecording search: Artist='%s', Recording='%s'\n\n", 
                   artist_name.c_str(), recording_name.c_str());
            
            SearchResult* result = mapping_search->search(artist_name, release_name, recording_name);
            
            if (result) {
                printf("Search Result:\n");
                printf("-------------\n");
                printf("Artist Credit: %-8d %s %s\n", 
                    result->artist_credit_id,
                    result->artist_credit_mbids.empty() ? "N/A" : result->artist_credit_mbids[0].c_str(),
                    result->artist_credit_name.c_str());
                printf("Release:       %-8d %s %s\n", 
                    result->release_id,
                    result->release_mbid.c_str(),
                    result->release_name.c_str());
                printf("Recording:     %-8d %s %s\n", 
                    result->recording_id,
                    result->recording_mbid.c_str(),
                    result->recording_name.c_str());
                
                string mbids = make_comma_sep_string(result->artist_credit_mbids);
                printf("\nFormatted output:\n");
                printf("{ \"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\" }\n", 
                    artist_name.c_str(), result->release_name.c_str(), recording_name.c_str(),
                    mbids.c_str(), result->release_mbid.c_str(), result->recording_mbid.c_str());
                
                delete result;
            } else {
                printf("No match found.\n");
            }
            printf("\n");
        }
        
        void search_artist(const string &query) {
            vector<IndexResult> *res = nullptr;
            
            // Try to encode the string normally first
            auto artist_name = encode.encode_string(query);
            if (artist_name.size()) {
                printf("ARTIST SEARCH: '%s' (%s)\n", query.c_str(), artist_name.c_str());
                res = artist_index->single_artist_index->search(artist_name, 0.5);
            }
            else {
                // Try encoding for "stupid artists" (non-Latin characters, etc.)
                auto stupid_name = encode.encode_string_for_stupid_artists(query);
                if (!stupid_name.size()) {
                    printf("Could not encode query: '%s'\n", query.c_str());
                    return;
                }
                
                printf("STUPID ARTIST SEARCH: '%s' (%s)\n", query.c_str(), stupid_name.c_str());
                res = artist_index->stupid_artist_index->search(stupid_name, 0.5);
            }
            
            if (!res->size()) {
                printf("  No results found.\n");
                delete res;
                return;
            }
            
            // Fetch artist credit IDs for all results
            map<unsigned int, vector<unsigned int>> artist_credit_ids = fetch_artist_credit_ids(*res);
            
            printf("\nResults:\n");
            printf("%-40s %-10s %-8s %-15s\n", "Name", "Confidence", "ID", "Artist Credits");
            printf("------------------------------------------------------------------------\n");
            
            for (auto &result : *res) {
                string text;
                if (artist_name.size()) {
                    text = artist_index->single_artist_index->get_index_text(result.result_index);
                } else {
                    text = artist_index->stupid_artist_index->get_index_text(result.result_index);
                }
                
                // Limit artist name to 4 characters
                string short_name = text.length() > 40 ? text.substr(0, 40) : text;
                
                // Format artist credit IDs
                string credit_ids_str = "";
                auto it = artist_credit_ids.find(result.id);
                if (it != artist_credit_ids.end()) {
                    for (size_t i = 0; i < it->second.size(); ++i) {
                        if (i > 0) credit_ids_str += ",";
                        credit_ids_str += to_string(it->second[i]);
                    }
                } else {
                    credit_ids_str = "none";
                }
                
                printf("%-40s %-10.2f %-8d %-15s\n", short_name.c_str(), result.confidence, result.id, credit_ids_str.c_str());
            }
            printf("\n");
            delete res;
        }

        void search_multiple_artist(const string &query) {
            vector<IndexResult> *res = nullptr;
            
            // Try to encode the string normally first
            auto artist_name = encode.encode_string(query);
            if (artist_name.size()) {
                printf("MULTIPLE ARTIST SEARCH: '%s' (%s)\n", query.c_str(), artist_name.c_str());
                res = artist_index->multiple_artist_index->search(artist_name, 0.5);
            }
            if (!res->size()) {
                printf("  No results found.\n");
                delete res;
                return;
            }
            
            printf("\nResults:\n");
            printf("%-40s %-10s %-8s\n", "Name", "Confidence", "Artist Credit");
            printf("------------------------------------------------------------------------\n");
            
            for (auto &result : *res) {
                string text;
                if (artist_name.size()) {
                    text = artist_index->multiple_artist_index->get_index_text(result.result_index);
                } else {
                    text = artist_index->multiple_artist_index->get_index_text(result.result_index);
                }
                string short_name = text.length() > 40 ? text.substr(0, 40) : text;
                printf("%-40s %-10.2f %-8d\n", short_name.c_str(), result.confidence, result.id);
            }
            printf("\n");
            delete res;
        }
        
        void dump_recordings_for_artist_credit(unsigned int artist_credit_id) {
            try {
                printf("\nLoading recordings for artist_credit_id: %u\n", artist_credit_id);
                
                // Query the database directly to show all recordings for this artist credit
                string db_file = index_dir + string("/mapping.db");
                SQLite::Database db(db_file);
                SQLite::Statement direct_query(db, "SELECT DISTINCT recording_id, recording_name FROM mapping WHERE artist_credit_id = ? ORDER BY recording_name");
                direct_query.bind(1, artist_credit_id);
                
                printf("\nRecordings:\n");
                printf("%-8s %s\n", "Rec ID", "Recording Name");
                printf("----------------------------------------------------\n");
                
                int count = 0;
                while (direct_query.executeStep()) {
                    unsigned int rec_id = direct_query.getColumn(0).getInt();
                    string rec_name = direct_query.getColumn(1).getText();
                    printf("%-8u %s\n", rec_id, rec_name.c_str());
                    count++;
                }
                printf("Total recordings: %d\n", count);
                
                if (count == 0) {
                    printf("No recordings found in mapping table for artist_credit_id: %u\n", artist_credit_id);
                }
                
                printf("\n");
                
            } catch (const std::exception& e) {
                printf("Error loading recordings for artist_credit_id %u: %s\n", artist_credit_id, e.what());
            }
        }
        
        void dump_releases_for_artist_credit(unsigned int artist_credit_id) {
            try {
                printf("\nLoading releases for artist_credit_id: %u\n", artist_credit_id);
                
                // Query the database directly to show all releases for this artist credit
                string db_file = index_dir + string("/mapping.db");
                SQLite::Database db(db_file);
                SQLite::Statement direct_query(db, "SELECT DISTINCT release_id, release_name FROM mapping WHERE artist_credit_id = ? ORDER BY release_name");
                direct_query.bind(1, artist_credit_id);
                
                printf("\nReleases:\n");
                printf("%-8s %s\n", "Rel ID", "Release Name");
                printf("----------------------------------------------------\n");
                
                int count = 0;
                while (direct_query.executeStep()) {
                    unsigned int rel_id = direct_query.getColumn(0).getInt();
                    string rel_name = direct_query.getColumn(1).getText();
                    printf("%-8u %s\n", rel_id, rel_name.c_str());
                    count++;
                }
                printf("Total releases: %d\n", count);
                
                if (count == 0) {
                    printf("No releases found in mapping table for artist_credit_id: %u\n", artist_credit_id);
                }
                
                printf("\n");
                
            } catch (const std::exception& e) {
                printf("Error loading releases for artist_credit_id %u: %s\n", artist_credit_id, e.what());
            }
        }
        
        void dump_index_release_data(unsigned int artist_credit_id) {
            try {
                printf("\nLoading recording index for artist_credit_id: %u\n", artist_credit_id);
                
                // Load the recording index for this artist credit
                ReleaseRecordingIndex *data = recording_index->load(artist_credit_id);
                
                if (!data) {
                    printf("Failed to load recording index for artist_credit_id: %u\n", artist_credit_id);
                    return;
                }
                
                printf("\n=== RELEASE DATA ===\n");
                if (data->release_index && !data->links.empty()) {
                    printf("%-8s %-50s %-10s %-8s\n", "Rel_Idx", "Release Text", "Rel_ID", "Rank");
                    printf("---------------------------------------------------------------------------------------------\n");
                    
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
                        printf("%-8u %-50s %-10u %-8u\n", 
                               entry.release_index,
                               entry.release_text.c_str(),
                               entry.release_id,
                               entry.rank);
                    }
                    
                    printf("Total release entries: %zu\n", release_entries.size());
                } else {
                    printf("No release data found.\n");
                }
                
                printf("\n");
                
                // Clean up
                delete data->recording_index;
                delete data->release_index;
                delete data;
                
            } catch (const std::exception& e) {
                printf("Error loading recording index for artist_credit_id %u: %s\n", artist_credit_id, e.what());
            }
        }
        
        void dump_index_recording_data(unsigned int artist_credit_id) {
            try {
                printf("\nLoading recording index for artist_credit_id: %u\n", artist_credit_id);
                
                // Load the recording index for this artist credit
                ReleaseRecordingIndex* data = recording_index->load(artist_credit_id);
                if (!data) {
                    printf("Failed to load recording index for artist_credit_id: %u\n", artist_credit_id);
                    return;
                }
                
                printf("\n=== RECORDING INDEX CONTENTS ===\n");
                if (data->recording_index) {
                    printf("Recording Index:\n");
                    printf("%-8s %s\n", "Rec ID", "Recording Text");
                    printf("----------------------------------------------------\n");
                    
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
                        printf("%-8u %s\n", recording.first, recording.second.c_str());
                    }
                    printf("Total recordings in index: %zu\n", recordings.size());
                } else {
                    printf("No recording index found.\n");
                }
                
                printf("\n");
                
                // Clean up
                delete data->recording_index;
                delete data->release_index;
                delete data;
                
            } catch (const std::exception& e) {
                printf("Error loading recording index for artist_credit_id %u: %s\n", artist_credit_id, e.what());
            }
        }
        
        void dump_links_data(unsigned int artist_credit_id) {
            try {
                printf("\nLoading recording index for artist_credit_id: %u\n", artist_credit_id);
                
                // Load the recording index for this artist credit
                ReleaseRecordingIndex *data = recording_index->load(artist_credit_id);
                
                if (!data) {
                    printf("Failed to load recording index for artist_credit_id: %u\n", artist_credit_id);
                    return;
                }
                
                printf("\n=== LINKS TABLE ===\n");
                if (!data->links.empty()) {
                    printf("%-10s %-8s %-21s %-10s %-8s %-8s %-21s\n", 
                           "Rec_Idx", "Rec_ID", "Recording Name", "Rel_Idx", "Rel_ID", "Rank", "Release Name");
                    printf("---------------------------------------------------------------------------------------------------------------\n");
                    
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
                                printf("%-10u %-8u %-21s %-10u %-8s %-8s %-21s\n", 
                                       link.recording_index, 
                                       link.recording_id,
                                       recording_name.c_str(),
                                       link.release_index, 
                                       release_id_str, 
                                       rank_str, 
                                       release_name.c_str());
                            } else if (release_id_str) {
                                printf("%-10u %-8u %-21s %-10u %-8s %-8u %-21s\n", 
                                       link.recording_index, 
                                       link.recording_id,
                                       recording_name.c_str(),
                                       link.release_index, 
                                       release_id_str, 
                                       link.rank, 
                                       release_name.c_str());
                            } else if (rank_str) {
                                printf("%-10u %-8u %-21s %-10u %-8u %-8s %-21s\n", 
                                       link.recording_index, 
                                       link.recording_id,
                                       recording_name.c_str(),
                                       link.release_index, 
                                       link.release_id, 
                                       rank_str, 
                                       release_name.c_str());
                            } else {
                                printf("%-10u %-8u %-21s %-10u %-8u %-8u %-21s\n", 
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
                        printf("Displayed %zu of %zu total links\n", lines_printed, total_links);
                    } else {
                        printf("Total links: %zu\n", total_links);
                    }
                } else {
                    printf("No links found.\n");
                }
                
                printf("\n");
                
                // Clean up
                delete data->recording_index;
                delete data->release_index;
                delete data;
                
            } catch (const std::exception& e) {
                printf("Error loading recording index for artist_credit_id %u: %s\n", artist_credit_id, e.what());
            }
        }
        
        string get_line_with_history() {
            char* input = readline("> ");
            
            if (input == nullptr) {
                // EOF (Ctrl-D)
                printf("\nGoodbye!\n");
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
            
            printf("Music Explorer Interactive Mode\n");
            printf("Index Directory: %s\n", index_dir.c_str());
            printf("\nCommands:\n");
            printf("  a <artist name>              - Search in single artist index\n");
            printf("  m <artist name>              - Search in multiple artist index\n");
            printf("  drec <artist_credit_id>      - Dump recordings for artist credit from SQLite\n");
            printf("  drel <artist_credit_id>      - Dump releases for artist credit from SQLite\n");
            printf("  rel <artist_credit_id>       - Dump recording index contents and release data\n");
            printf("  rec <artist_credit_id>       - Dump recording index IDs and strings\n");
            printf("  l <artist_credit_id>         - Dump links table for artist credit\n");
            printf("  s <artist>, <release>, <rec> - Full search: artist + release + recording\n");
            printf("  rs <artist>, <recording>     - Recording search: artist + recording (no release)\n");
            printf("  q, .q, \\q, quit, exit        - Quit the program\n");
            printf("\nUse Up/Down arrow keys for command history, Left/Right/Home/End for editing.\n");
            printf("Ctrl+A (beginning), Ctrl+E (end), Ctrl+K (kill to end), Ctrl+U (kill to beginning)\n\n");
            
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
                        printf("Usage: a <artist name>\n");
                    }
                } else if (input.substr(0, 2) == "m ") {
                    string artist_query = input.substr(2);
                    if (!artist_query.empty()) {
                        search_multiple_artist(artist_query);
                    } else {
                        printf("Usage: m <artist name>\n");
                    }
                } else if (input.substr(0, 5) == "drec ") {
                    string id_str = input.substr(5);
                    try {
                        unsigned int artist_credit_id = stoul(id_str);
                        dump_recordings_for_artist_credit(artist_credit_id);
                    } catch (const std::exception& e) {
                        printf("Invalid artist_credit_id: '%s'. Please enter a valid number.\n", id_str.c_str());
                    }
                } else if (input.substr(0, 5) == "drel ") {
                    string id_str = input.substr(5);
                    try {
                        unsigned int artist_credit_id = stoul(id_str);
                        dump_releases_for_artist_credit(artist_credit_id);
                    } catch (const std::exception& e) {
                        printf("Invalid artist_credit_id: '%s'. Please enter a valid number.\n", id_str.c_str());
                    }
                } else if (input.substr(0, 4) == "rel ") {
                    string id_str = input.substr(4);
                    try {
                        unsigned int artist_credit_id = stoul(id_str);
                        dump_index_release_data(artist_credit_id);
                    } catch (const std::exception& e) {
                        printf("Invalid artist_credit_id: '%s'. Please enter a valid number.\n", id_str.c_str());
                    }
                } else if (input.substr(0, 4) == "rec ") {
                    string id_str = input.substr(4);
                    try {
                        unsigned int artist_credit_id = stoul(id_str);
                        dump_index_recording_data(artist_credit_id);
                    } catch (const std::exception& e) {
                        printf("Invalid artist_credit_id: '%s'. Please enter a valid number.\n", id_str.c_str());
                    }
                } else if (input.substr(0, 5) == "irec ") {
                    string id_str = input.substr(5);
                    try {
                        unsigned int artist_credit_id = stoul(id_str);
                        dump_index_recording_data(artist_credit_id);
                    } catch (const std::exception& e) {
                        printf("Invalid artist_credit_id: '%s'. Please enter a valid number.\n", id_str.c_str());
                    }
                } else if (input.substr(0, 2) == "l ") {
                    string id_str = input.substr(2);
                    try {
                        unsigned int artist_credit_id = stoul(id_str);
                        dump_links_data(artist_credit_id);
                    } catch (const std::exception& e) {
                        printf("Invalid artist_credit_id: '%s'. Please enter a valid number.\n", id_str.c_str());
                    }
                } else if (input.substr(0, 2) == "s ") {
                    string search_query = input.substr(2);
                    if (!search_query.empty()) {
                        full_search(search_query);
                    } else {
                        printf("Usage: s <artist>, <release>, <recording>\n");
                        printf("Examples:\n");
                        printf("  s portishead, mezzanine, teardrop\n");
                        printf("  s björk, homogenic, joga\n");
                        printf("For artist + recording only (no release), use: rs <artist>, <recording>\n");
                    }
                } else if (input.substr(0, 3) == "rs ") {
                    string search_query = input.substr(3);
                    if (!search_query.empty()) {
                        recording_search(search_query);
                    } else {
                        printf("Usage: rs <artist>, <recording>\n");
                        printf("Examples:\n");
                        printf("  rs portishead, teardrop\n");
                        printf("  rs björk, joga\n");
                    }
                } else {
                    printf("Unknown command: '%s'\n", input.c_str());
                    printf("Available commands: a <artist>, rec <id>, rel <id>, irel <id>, j <id>, s <artist>,<release>[,<recording>], rs <artist>,<recording>, q/quit/\\q/.q\n");
                }
            }
        }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: explore <index_dir>\n");
        printf("  index_dir: Directory containing the mapping database and index files\n");
        printf("\nInteractive music database explorer with artist search and recording/release lookup.\n");
        return -1;
    }
    
    try {
        string index_dir = argv[1];
        Explorer explorer(index_dir);
        
        printf("Loading artist index from: %s\n", index_dir.c_str());
        explorer.load();
        printf("Explorer ready.\n\n");
        
        explorer.run_interactive();
        
    } catch (const std::exception& e) {
        printf("Error: %s\n", e.what());
        return -1;
    }
    
    return 0;
}
