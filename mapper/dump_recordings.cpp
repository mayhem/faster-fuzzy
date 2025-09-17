#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>

#include "SQLiteCpp.h"
#include "recording_index.hpp"
#include "encode.hpp"
#include "utils.hpp"

using namespace std;

class RecordingDumper {
    private:
        string            index_dir;
        RecordingIndex   *recording_index;

    public:
        RecordingDumper(const string &_index_dir) {
            index_dir = _index_dir;
            recording_index = new RecordingIndex(index_dir);
        }
        
        ~RecordingDumper() {
            delete recording_index;
        }
        
        void dump_recordings_for_artist_credit(unsigned int artist_credit_id) {
            try {
                printf("\nLoading cached indexes for artist_credit_id: %u\n", artist_credit_id);
                
                // Load the recording/release indexes from cache instead of building new ones
                FuzzyIndex *rec_index = new FuzzyIndex();
                FuzzyIndex *rel_index = new FuzzyIndex();
                vector<IndexSupplementalReleaseData> *release_data = new vector<IndexSupplementalReleaseData>();
                
                // Try to load from index_cache table using RecordingIndex
                bool loaded = false;
                try {
                    this->recording_index->load_index(artist_credit_id, rec_index, rel_index, release_data);
                    loaded = true;
                } catch (const std::exception& e) {
                    printf("Could not load from cache, trying build method: %s\n", e.what());
                    // Fallback to building indexes if cache loading fails
                    delete rec_index;
                    delete rel_index;
                    delete release_data;
                    
                    ArtistReleaseRecordingData data = this->recording_index->build_recording_release_indexes(artist_credit_id);
                    rec_index = data.recording_index;
                    rel_index = data.release_index;
                    release_data = data.release_data;
                    loaded = (rec_index != nullptr || rel_index != nullptr);
                }
                
                if (!loaded) {
                    printf("No cached index found for artist_credit_id: %u\n", artist_credit_id);
                    return;
                }
                
                // Display recording index information
                if (rec_index) {
                    printf("\nRecordings (from cached index):\n");
                    printf("%-8s %s\n", "Rec ID", "Recording Name");
                    printf("----------------------------------------------------\n");
                    
                    // Access the private index_ids and index_texts vectors through search results
                    // We'll do a dummy search to get all items
                    vector<IndexResult> dummy_results = rec_index->search("", 0.0);  // Low threshold to get more results
                    
                    for (const auto& result : dummy_results) {
                        string text = rec_index->get_index_text(result.result_index);
                        printf("%-8u %s\n", result.id, text.c_str());
                    }
                    printf("Total recordings found: %lu\n", dummy_results.size());
                } else {
                    printf("No recording index found in cache.\n");
                }
                
                // Display release index information
                if (rel_index) {
                    printf("\nReleases (from cached index):\n");
                    printf("%-8s %s\n", "Rel ID", "Release Name");
                    printf("----------------------------------------------------\n");
                    
                    // Access the private index_ids and index_texts vectors through search results
                    vector<IndexResult> dummy_results = rel_index->search("", 0.0);  // Low threshold to get more results
                    
                    for (const auto& result : dummy_results) {
                        string text = rel_index->get_index_text(result.result_index);
                        printf("%-8u %s\n", result.id, text.c_str());
                    }
                    printf("Total releases found: %lu\n", dummy_results.size());
                } else {
                    printf("No release index found in cache.\n");
                }
                
                // Display supplemental release data if available
                if (release_data && !release_data->empty()) {
                    printf("\nSupplemental Release Data (from cached index):\n");
                    printf("%-8s %-8s %s\n", "Ref ID", "Rec ID", "Recording Name");
                    printf("----------------------------------------------------\n");
                    
                    // Collect all unique recording IDs first
                    set<unsigned int> recording_ids;
                    for (const auto& rel_data : *release_data) {
                        for (const auto& ref : rel_data.release_refs) {
                            recording_ids.insert(ref.rank);  // Use rank as recording_id
                        }
                    }
                    
                    // Build a map of recording_id -> recording_name with a single query
                    map<unsigned int, string> recording_names;
                    if (!recording_ids.empty()) {
                        string db_file = index_dir + string("/mapping.db");
                        SQLite::Database db(db_file);
                        
                        // Build IN clause for the query
                        string in_clause = "(";
                        bool first = true;
                        for (unsigned int id : recording_ids) {
                            if (!first) in_clause += ",";
                            in_clause += to_string(id);
                            first = false;
                        }
                        in_clause += ")";
                        
                        string query_str = "SELECT recording_id, recording_name FROM mapping WHERE recording_id IN " + in_clause;
                        
                        try {
                            SQLite::Statement query(db, query_str);
                            while (query.executeStep()) {
                                unsigned int rec_id = query.getColumn(0).getInt();
                                string rec_name = query.getColumn(1).getText();
                                recording_names[rec_id] = rec_name;
                            }
                        } catch (const std::exception& e) {
                            printf("Error fetching recording names: %s\n", e.what());
                        }
                    }
                    
                    // Display the results using the fetched names
                    for (const auto& rel_data : *release_data) {
                        for (const auto& ref : rel_data.release_refs) {
                            unsigned int recording_id = ref.rank;
                            string recording_name = "[ not found in DB ]";
                            
                            auto it = recording_names.find(recording_id);
                            if (it != recording_names.end()) {
                                recording_name = it->second;
                            }
                            
                            printf("%-8u %-8u %s\n", ref.id, recording_id, recording_name.c_str());
                        }
                    }
                } else {
                    printf("No supplemental release data found in cache.\n");
                }
                
                // Cleanup allocated memory
                if (rec_index) delete rec_index;
                if (rel_index) delete rel_index;
                if (release_data) delete release_data;
                
                printf("\n");
                
            } catch (const std::exception& e) {
                printf("Error loading recordings for artist_credit_id %u: %s\n", artist_credit_id, e.what());
            }
        }
        
        void run_interactive() {
            string input;
            
            printf("Recording Dumper Interactive Mode\n");
            printf("Index Directory: %s\n", index_dir.c_str());
            printf("Type '\\q', 'quit', or 'exit' to quit.\n\n");
            
            while (true) {
                printf("> ");
                fflush(stdout);
                
                if (!getline(cin, input)) {
                    // Handle EOF (Ctrl-D) or input error
                    printf("\nGoodbye!\n");
                    break;
                }
                
                // Trim whitespace
                input.erase(0, input.find_first_not_of(" \t\n\r\f\v"));
                input.erase(input.find_last_not_of(" \t\n\r\f\v") + 1);
                
                if (input.empty()) {
                    continue;
                }
                
                if (input == "\\q" || input == "quit" || input == "exit") {
                    printf("Goodbye!\n");
                    break;
                }
                
                // Try to parse the input as an artist_credit_id
                try {
                    unsigned int artist_credit_id = stoul(input);
                    dump_recordings_for_artist_credit(artist_credit_id);
                } catch (const std::exception& e) {
                    printf("Invalid artist_credit_id: '%s'. Please enter a valid number.\n", input.c_str());
                }
            }
        }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: dump_recordings <index_dir>\n");
        printf("  index_dir: Directory containing the mapping database and index files\n");
        printf("\nEnter artist_credit_id values to dump associated recordings and releases.\n");
        return -1;
    }
    
    try {
        string index_dir = argv[1];
        RecordingDumper dumper(index_dir);
        
        printf("Recording dumper initialized with index directory: %s\n\n", index_dir.c_str());
        
        dumper.run_interactive();
        
    } catch (const std::exception& e) {
        printf("Error: %s\n", e.what());
        return -1;
    }
    
    return 0;
}