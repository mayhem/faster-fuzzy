#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>

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
            
            printf("\nResults:\n");
            printf("%-10s %-8s %s\n", "Confidence", "ID", "Artist Name");
            printf("------------------------------------------------------\n");
            
            for (auto &result : res) {
                string text;
                if (artist_name.size()) {
                    text = artist_index->artist_index->get_index_text(result.result_index);
                } else {
                    text = artist_index->stupid_artist_index->get_index_text(result.result_index);
                }
                
                printf("%-10.2f %-8d %s\n", result.confidence, result.id, text.c_str());
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
                getline(cin, query);
                
                // Trim whitespace
                query.erase(0, query.find_first_not_of(" \t\n\r\f\v"));
                query.erase(query.find_last_not_of(" \t\n\r\f\v") + 1);
                
                if (query.empty()) {
                    continue;
                }
                
                if (query == "\\q") 
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
