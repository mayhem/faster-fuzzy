#include <stdio.h>

#include "artist_index.hpp"
#include "mbid_mapping.hpp"
#include "utils.hpp"
#include "SQLiteCpp.h"

int main(int argc, char *argv[])
{
    bool skip_artists = false;
    bool force_rebuild = false;
    string index_dir;
    
    if (argc < 2 || argc > 4) {
        printf("Usage: mapping_create [--skip-artists] [--force-rebuild] <index_dir>\n");
        printf("  --skip-artists   Skip building artist indexes\n");
        printf("  --force-rebuild  Force rebuild all recording indexes (ignore cache)\n");
        return -1;
    }
    
    // Parse arguments
    int arg_index = 1;
    while (arg_index < argc - 1) {
        string arg = string(argv[arg_index]);
        if (arg == "--skip-artists") {
            skip_artists = true;
        } else if (arg == "--force-rebuild") {
            force_rebuild = true;
        } else {
            printf("Unknown option: %s\n", argv[arg_index]);
            printf("Usage: mapping_create [--skip-artists] [--force-rebuild] <index_dir>\n");
            return -1;
        }
        arg_index++;
    }
    
    // Last argument should be the index directory
    index_dir = string(argv[argc - 1]);
    
    // Validate flag combinations
    if (skip_artists && force_rebuild) {
        printf("Error: --skip-artists and --force-rebuild cannot be used together\n");
        printf("Usage: mapping_create [--skip-artists] [--force-rebuild] <index_dir>\n");
        return -1;
    }
   
    // Clear cache if force rebuild is requested
    if (force_rebuild) {
        log("force rebuild requested - clearing index cache");
        try {
            string db_file = index_dir + "/mapping.db";
            SQLite::Database db(db_file, SQLite::OPEN_READWRITE);
            db.exec("DELETE FROM index_cache");
            log("index cache cleared successfully");
        } catch (const std::exception& e) {
            printf("Error clearing index cache: %s\n", e.what());
            return -1;
        }
    }

    if (!skip_artists) {
        log("build artist indexes");
        ArtistIndex *artist_index = new ArtistIndex(index_dir);
        artist_index->build();
        // clean up to free memory
        delete artist_index;
    } else {
        log("skipping artist indexes (--skip-artists specified)");
    }

    log("build recording indexes");
    MBIDMapping mapping(index_dir);
    mapping.build_recording_indexes();

    return 0;
}