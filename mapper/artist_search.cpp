#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>

#include "SQLiteCpp.h"
#include "artist_index.hpp"
#include "encode.hpp"
#include "utils.hpp"

using namespace std;

class ArtistSearch {
    private:
        string              index_dir;
        ArtistIndex        *artist_index;
        EncodeSearchData    encode;

    public:
        ArtistSearch(const string &_index_dir) {
            index_dir = _index_dir;
            artist_index = new ArtistIndex(index_dir);
        }
        
        ~ArtistSearch() {
            delete artist_index;
        }
        
        void load() {
            if (!artist_index->load()) {
                throw std::runtime_error("Failed to load artist index");
            }
        }
        
        map<unsigned int, vector<unsigned int>>
        fetch_artist_credit_ids(const vector<IndexResult> &res) {
            map<unsigned int, vector<unsigned int>> artist_credit_map;
            string db_file = index_dir + string("/mapping.db");
            
            try {
                SQLite::Database db(db_file);
                
                // Create a set of unique artist_ids to query
                set<unsigned int> artist_ids;
                for (const auto& result : res) {
                    artist_ids.insert(result.id);
                }
                
                // Query the artist_credit_mapping table for each artist_id
                for (unsigned int artist_id : artist_ids) {
                    SQLite::Statement query(db, "SELECT artist_credit_id FROM artist_credit_mapping WHERE artist_id = ?");
                    query.bind(1, artist_id);
                    
                    vector<unsigned int> credit_ids;
                    while (query.executeStep()) {
                        credit_ids.push_back(query.getColumn(0).getInt());
                    }
                    
                    if (!credit_ids.empty()) {
                        artist_credit_map[artist_id] = credit_ids;
                    }
                }
            }
            catch (std::exception& e) {
                printf("Error fetching artist credit IDs: %s\n", e.what());
            }
            
            return artist_credit_map;
        }
        
        void search_artist(const string &query) {
            vector<IndexResult> res;
            
            // Try to encode the string normally first
            auto artist_name = encode.encode_string(query);
            if (artist_name.size()) {
                printf("ARTIST SEARCH: '%s' (%s)\n", query.c_str(), artist_name.c_str());
                res = artist_index->artist_index->search(artist_name, 0.5);
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
            
            if (!res.size()) {
                printf("  No results found.\n");
                return;
            }
            
            // Fetch artist credit IDs for all results
            map<unsigned int, vector<unsigned int>> artist_credit_ids = fetch_artist_credit_ids(res);
            
            printf("\nResults:\n");
            printf("%-40s %-10s %-8s %-15s\n", "name", "confidence", "artist_id", "artist_credit_ids");
            printf("------------------------------------------------------------------------\n");
            
            for (auto &result : res) {
                string text;
                if (artist_name.size()) {
                    text = artist_index->artist_index->get_index_text(result.result_index);
                } else {
                    text = artist_index->stupid_artist_index->get_index_text(result.result_index);
                }
                
                // Limit artist name to 40 characters
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
        }
        
        void run_interactive() {
            string query;
            
            printf("Artist Search Interactive Mode\n");
            printf("Index Directory: %s\n", index_dir.c_str());
            printf("Type '\\q' to quit.\n\n");
            
            while (true) {
                printf("> ");
                fflush(stdout);
                
                if (!getline(cin, query))
                    break;
                
                // Trim whitespace
                query.erase(0, query.find_first_not_of(" \t\n\r\f\v"));
                query.erase(query.find_last_not_of(" \t\n\r\f\v") + 1);
                
                if (query.empty() || query == "\\q") 
                    break;
                
                search_artist(query);
            }
        }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: artist_search <index_dir>\n");
        printf("  index_dir: Directory containing the artist index files\n");
        return -1;
    }
    
    try {
        string index_dir = argv[1];
        ArtistSearch searcher(index_dir);
        
        printf("Loading artist index from: %s\n", index_dir.c_str());
        searcher.load();
        printf("Artist index loaded successfully.\n\n");
        
        searcher.run_interactive();
        
    } catch (const std::exception& e) {
        printf("Error: %s\n", e.what());
        return -1;
    }
    
    return 0;
}
