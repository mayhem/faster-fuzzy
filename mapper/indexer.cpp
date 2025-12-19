#include <stdio.h>

#include "artist_index.hpp"
#include "mbid_mapping.hpp"
#include "utils.hpp"
#include "SQLiteCpp.h"

int main(int argc, char *argv[])
{
    init_logging();
    
    bool skip_artists = false;
    bool force_rebuild = false;
    string index_dir;
    
    // Get index_dir from environment variable first
    const char* env_index_dir = std::getenv("INDEX_DIR");
    if (env_index_dir && strlen(env_index_dir) > 0) {
        index_dir = env_index_dir;
    }
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--skip-artists") {
            skip_artists = true;
        } else if (arg == "--force-rebuild") {
            force_rebuild = true;
        } else if (arg.rfind("--", 0) == 0) {
            log("Unknown option: %s", arg.c_str());
            log("Usage: indexer [--skip-artists] [--force-rebuild] [<index_dir>]");
            log("  --skip-artists   Skip building artist indexes");
            log("  --force-rebuild  Force rebuild all recording indexes (ignore cache)");
            log("  index_dir can also be set via INDEX_DIR environment variable");
            return -1;
        } else {
            // Command line argument overrides environment variable
            index_dir = arg;
        }
    }
    
    if (index_dir.empty()) {
        log("Error: Missing index directory");
        log("Usage: indexer [--skip-artists] [--force-rebuild] [<index_dir>]");
        log("  index_dir can also be set via INDEX_DIR environment variable");
        return -1;
    }
    
    // Validate flag combinations
    if (skip_artists && force_rebuild) {
        log("Error: --skip-artists and --force-rebuild cannot be used together");
        log("Usage: mapping_create [--skip-artists] [--force-rebuild] <index_dir>");
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
            log("Error clearing index cache: %s", e.what());
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