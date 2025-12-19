#include "create_base_index.hpp"
#include "artist_index.hpp"
#include "mbid_mapping.hpp"
#include <libpq-fe.h>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <filesystem>

const char* MAPPING_QUERY = R"(
         SELECT rec.artist_credit AS artist_credit_id
              , artist_mbids::TEXT[]
              , artist_credit_name
              , COALESCE(array_agg(a.sort_name ORDER BY acn.position)) as artist_credit_sortname
              , rel.id AS release_id
              , rel.gid::TEXT AS release_mbid
              , rel.artist_credit AS release_artist_credit_id
              , release_name
              , rec.id AS recording_id
              , rec.gid::TEXT AS recording_mbid
              , recording_name
              , score
           FROM mapping.canonical_musicbrainz_data_release_support
           JOIN recording rec
             ON rec.gid = recording_mbid
           JOIN release rel
             ON rel.gid = release_mbid
           JOIN artist_credit_name acn
             ON artist_credit_id = acn.artist_credit
           JOIN artist a
             ON acn.artist = a.id
          WHERE artist_credit_id > 1
       GROUP BY artist_credit_id
              , artist_mbids
              , artist_credit_name
              , release_name
              , rel.id
              , rel.artist_credit
              , recording_name
              , rec.id
              , score
      UNION
           SELECT r.artist_credit as artist_credit_id
                , array_agg(a.gid::TEXT) as artist_mbids
                , acn."name" as artist_credit_name
                , COALESCE(array_agg(a.sort_name ORDER BY acn.position)) as artist_credit_sortname
                , 9223372036854775807 AS release_id
                , '' AS release_mbid
                , 9223372036854775807 AS release_artist_credit_id
                , '' AS release_name
                , r.id AS recording_id
                , r.gid::TEXT AS recording_mbid
                , r.name
                , 9223372036854775807
             FROM recording r
        LEFT JOIN track t
               ON t.recording = r.id
             JOIN artist_credit_name acn
               ON r.artist_credit = acn.artist_credit
             JOIN artist a
               ON acn.artist = a.id
            WHERE t.id IS null
         GROUP BY r.artist_credit
                , artist_credit_name
                , release_name
                , r.name
                , r.id
)";

CreateBaseIndex::CreateBaseIndex(const string& _index_dir) : index_dir(_index_dir) {
}

void CreateBaseIndex::create() {
    auto t0 = std::chrono::high_resolution_clock::now();
    
    // Create index directory if it doesn't exist
    std::filesystem::create_directories(index_dir);
    
    string db_file = index_dir + "/mapping.db";
    string csv_file = index_dir + "/import.csv";
    
    // Create SQLite database
    create_sqlite_db(db_file);
    
    log("Connecting to PostgreSQL...");
    
    // Get DB connection string from environment variable
    const char* db_connect = std::getenv("CANONICAL_MUSICBRAINZ_DATA_CONNECT");
    if (!db_connect || strlen(db_connect) == 0) {
        throw std::runtime_error("CANONICAL_MUSICBRAINZ_DATA_CONNECT environment variable not set");
    }
    
    // Connect to PostgreSQL using libpq
    PGconn *conn = PQconnectdb(db_connect);
    
    if (PQstatus(conn) != CONNECTION_OK) {
        log("Connection to database failed: %s", PQerrorMessage(conn));
        PQfinish(conn);
        throw std::runtime_error("PostgreSQL connection failed");
    }
    
    log("Executing query...");
    
    // Start a transaction for the cursor
    PGresult *result = PQexec(conn, "BEGIN");
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        log("BEGIN failed: %s", PQerrorMessage(conn));
        PQclear(result);
        PQfinish(conn);
        throw std::runtime_error("PostgreSQL BEGIN failed");
    }
    PQclear(result);
    
    // Declare a cursor for the big query
    string cursor_sql = "DECLARE big_cursor CURSOR FOR " + string(MAPPING_QUERY);
    result = PQexec(conn, cursor_sql.c_str());
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        log("DECLARE CURSOR failed: %s", PQerrorMessage(conn));
        PQclear(result);
        PQfinish(conn);
        throw std::runtime_error("PostgreSQL DECLARE CURSOR failed");
    }
    PQclear(result);
    
    log("Writing CSV file...");
    ofstream csvfile(csv_file);
    if (!csvfile.is_open()) {
        PQclear(result);
        PQfinish(conn);
        throw std::runtime_error("Failed to open CSV file for writing");
    }
    
    // Write CSV header (not needed for import, but good for debugging)
    csvfile << "artist_credit_id,artist_mbids,artist_credit_name,artist_credit_sortname,"
            << "release_id,release_mbid,release_artist_credit_id,release_name,"
            << "recording_id,recording_mbid,recording_name,score" << endl;
    
    vector<MappingRow> mapping_data;
    int row_count = 0;
    
    // Fetch data in batches using the cursor
    while (true) {
        string fetch_sql = "FETCH " + to_string(NUM_ROWS_PER_COMMIT) + " FROM big_cursor";
        result = PQexec(conn, fetch_sql.c_str());
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            log("FETCH failed: %s", PQerrorMessage(conn));
            PQclear(result);
            PQfinish(conn);
            throw std::runtime_error("PostgreSQL FETCH failed");
        }
        
        int num_rows = PQntuples(result);
        if (num_rows == 0) {
            // No more rows
            PQclear(result);
            break;
        }
        
        for (int i = 0; i < num_rows; i++) {
            MappingRow mrow;
            
            // Check for NULL values and provide defaults
            char* val = PQgetvalue(result, i, 0);
            mrow.artist_credit_id = (val && *val) ? strtoul(val, nullptr, 10) : 0;
            
            // Handle PostgreSQL array - convert to comma-separated string
            val = PQgetvalue(result, i, 1);
            string mbids_raw = val ? val : "";
            // Simple array parsing - remove { } and split by comma
            if (mbids_raw.length() > 2 && mbids_raw[0] == '{' && mbids_raw.back() == '}') {
                mbids_raw = mbids_raw.substr(1, mbids_raw.length() - 2);
            }
            mrow.artist_mbids = mbids_raw;
            
            val = PQgetvalue(result, i, 2);
            mrow.artist_credit_name = val ? val : "";
            
            // Handle sortname array - simple parsing
            val = PQgetvalue(result, i, 3);
            string sortname_raw = val ? val : "";
            if (sortname_raw.length() > 2 && sortname_raw[0] == '{' && sortname_raw.back() == '}') {
                sortname_raw = sortname_raw.substr(1, sortname_raw.length() - 2);
                size_t comma_pos = sortname_raw.find(',');
                if (comma_pos != string::npos) {
                    sortname_raw = sortname_raw.substr(0, comma_pos);
                }
            }
            mrow.artist_credit_sortname = sortname_raw;
            
            val = PQgetvalue(result, i, 4);
            mrow.release_id = (val && *val) ? strtoul(val, nullptr, 10) : 0;
            val = PQgetvalue(result, i, 5);
            mrow.release_mbid = val ? val : "";
            val = PQgetvalue(result, i, 6);
            mrow.release_artist_credit_id = (val && *val) ? strtoul(val, nullptr, 10) : 0;
            val = PQgetvalue(result, i, 7);
            mrow.release_name = val ? val : "";
            val = PQgetvalue(result, i, 8);
            mrow.recording_id = (val && *val) ? strtoul(val, nullptr, 10) : 0;
            val = PQgetvalue(result, i, 9);
            mrow.recording_mbid = val ? val : "";
            val = PQgetvalue(result, i, 10);
            mrow.recording_name = val ? val : "";
            val = PQgetvalue(result, i, 11);
            mrow.score = (val && *val) ? strtoul(val, nullptr, 10) : 0;
            
            // Write directly to CSV instead of buffering
            csvfile << mrow.artist_credit_id << ","
                   << escape_csv_field(mrow.artist_mbids) << ","
                   << escape_csv_field(mrow.artist_credit_name) << ","
                   << escape_csv_field(mrow.artist_credit_sortname) << ","
                   << mrow.release_id << ","
                   << escape_csv_field(mrow.release_mbid) << ","
                   << mrow.release_artist_credit_id << ","
                   << escape_csv_field(mrow.release_name) << ","
                   << mrow.recording_id << ","
                   << escape_csv_field(mrow.recording_mbid) << ","
                   << escape_csv_field(mrow.recording_name) << ","
                   << mrow.score << endl;
            
            row_count++;
        }
        
        PQclear(result);
    }
    
    // Close cursor and end transaction
    result = PQexec(conn, "CLOSE big_cursor");
    PQclear(result);
    result = PQexec(conn, "COMMIT");
    PQclear(result);
    
    csvfile.close();
    PQfinish(conn);
    log("\nWrote %d rows to CSV", row_count);
    
    log("Importing CSV into SQLite...");
    import_csv_to_sqlite(db_file, csv_file);
    
    log("Creating indexes...");
    create_indexes(db_file);
    
    // Clean up CSV file
    if (std::filesystem::remove(csv_file)) {
        log("Removed temporary CSV file");
    } else {
        log("Warning: Could not remove temporary CSV file: %s", csv_file.c_str());
    }
    
    auto t1 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
    log("\nLoaded data and saved in %.3f seconds.", duration.count() / 1000.0);
}

void CreateBaseIndex::create_sqlite_db(const string& db_file) {
    // Remove existing database
    std::filesystem::remove(db_file);
    
    try {
        SQLite::Database db(db_file, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        
        // Create mapping table
        db.exec(R"(
            CREATE TABLE mapping (
                artist_credit_id INTEGER NOT NULL,
                artist_mbids TEXT NOT NULL,
                artist_credit_name TEXT NOT NULL,
                artist_credit_sortname TEXT,
                release_id INTEGER NOT NULL,
                release_mbid TEXT NOT NULL,
                release_artist_credit_id INTEGER NOT NULL,
                release_name TEXT,
                recording_id INTEGER NOT NULL,
                recording_mbid TEXT NOT NULL,
                recording_name TEXT,
                score INTEGER NOT NULL
            )
        )");
        
        // Create index_cache table
        db.exec(R"(
            CREATE TABLE index_cache (
                entity_id INTEGER NOT NULL UNIQUE,
                index_data BLOB NOT NULL
            )
        )");
        
        // Create index on index_cache.entity_id
        db.exec("CREATE INDEX entity_id_idx ON index_cache(entity_id)");
        
    } catch (const std::exception& e) {
        log("SQLite database creation error: %s", e.what());
        throw;
    }
}

void CreateBaseIndex::import_csv_to_sqlite(const string& db_file, const string& csv_file) {
    try {
        SQLite::Database db(db_file, SQLite::OPEN_READWRITE);
        
        // Set up for CSV import
        db.exec("PRAGMA synchronous = OFF");
        db.exec("PRAGMA journal_mode = MEMORY");
        
        // Prepare import statement
        string import_sql = "INSERT INTO mapping VALUES (?,?,?,?,?,?,?,?,?,?,?,?)";
        SQLite::Statement stmt(db, import_sql);
        
        ifstream csvfile(csv_file);
        if (!csvfile.is_open()) {
            throw std::runtime_error("Failed to open CSV file for reading");
        }
        
        string line;
        getline(csvfile, line); // Skip header
        
        db.exec("BEGIN TRANSACTION");
        
        int count = 0;
        while (getline(csvfile, line)) {
            if (line.empty()) continue;
            
            // Proper CSV parsing that handles quoted fields with commas
            vector<string> fields;
            size_t pos = 0;
            bool in_quotes = false;
            string field = "";
            
            while (pos < line.length()) {
                char c = line[pos];
                
                if (c == '"') {
                    if (in_quotes && pos + 1 < line.length() && line[pos + 1] == '"') {
                        // Escaped quote ""
                        field += '"';
                        pos += 2;
                    } else {
                        // Start or end of quoted field
                        in_quotes = !in_quotes;
                        pos++;
                    }
                } else if (c == ',' && !in_quotes) {
                    // Field separator
                    fields.push_back(field);
                    field = "";
                    pos++;
                } else {
                    // Regular character
                    field += c;
                    pos++;
                }
            }
            
            // Add the last field
            fields.push_back(field);
            
            if (fields.size() != 12) {
                log("Invalid CSV line (expected 12 fields): %s", line.c_str());
                continue;
            }
            
            try {
                stmt.bind(1, static_cast<int64_t>(stoul(fields[0])));  // artist_credit_id
                stmt.bind(2, fields[1]);         // artist_mbids
                stmt.bind(3, fields[2]);         // artist_credit_name
                stmt.bind(4, fields[3]);         // artist_credit_sortname
                stmt.bind(5, static_cast<int64_t>(stoul(fields[4])));  // release_id
                stmt.bind(6, fields[5]);         // release_mbid
                stmt.bind(7, static_cast<int64_t>(stoul(fields[6])));  // release_artist_credit_id
                stmt.bind(8, fields[7]);         // release_name
                stmt.bind(9, static_cast<int64_t>(stoul(fields[8])));  // recording_id
                stmt.bind(10, fields[9]);        // recording_mbid
                stmt.bind(11, fields[10]);       // recording_name
                stmt.bind(12, static_cast<int64_t>(stoul(fields[11]))); // score
            } catch (const std::invalid_argument& e) {
                log("Invalid integer in CSV line: %s", line.c_str());
                continue;
            } catch (const std::out_of_range& e) {
                log("Integer out of range in CSV line: %s", line.c_str());
                continue;
            }
            
            stmt.exec();
            stmt.reset();
            
            count++;
            if (count % 100000 == 0) {
                fprintf(stdout, "\rImported %d rows in sqlite", count);
                fflush(stdout);
            }
        }
        
        db.exec("COMMIT");
        csvfile.close();
        
        log("\nImported %d total rows", count);
        
    } catch (const std::exception& e) {
        log("CSV import error: %s", e.what());
        throw;
    }
}

void CreateBaseIndex::create_indexes(const string& db_file) {
    try {
        SQLite::Database db(db_file, SQLite::OPEN_READWRITE);
        
        log("Creating artist_credit_id index...");
        db.exec("CREATE INDEX artist_credit_id_ndx ON mapping(artist_credit_id)");
        
        log("Creating release_artist_credit_id index...");
        db.exec("CREATE INDEX release_artist_credit_id_ndx ON mapping(release_artist_credit_id)");
        
        log("Creating release_id index...");
        db.exec("CREATE INDEX release_id_ndx ON mapping(release_id)");
        
        log("Creating recording_id index...");
        db.exec("CREATE INDEX recording_id_ndx ON mapping(recording_id)");
        
        log("Creating release_id_recording_id composite index...");
        db.exec("CREATE INDEX release_id_recording_id_ndx ON mapping(release_id, recording_id)");
        
        log("All indexes created successfully");
        
    } catch (const std::exception& e) {
        log("Index creation error: %s", e.what());
        throw;
    }
}

string CreateBaseIndex::escape_csv_field(const string& field) {
    if (field.empty()) return "\"\"";
    
    bool needs_quotes = field.find(',') != string::npos ||
                       field.find('"') != string::npos ||
                       field.find('\n') != string::npos ||
                       field.find('\r') != string::npos;
    
    if (!needs_quotes) return field;
    
    string escaped = "\"";
    for (char c : field) {
        if (c == '"') {
            escaped += "\"\"";  // Escape quotes by doubling them
        } else {
            escaped += c;
        }
    }
    escaped += "\"";
    
    return escaped;
}

int main(int argc, char *argv[])
{
    init_logging();
    
    bool build_indexes = false;
    string index_dir;
    
    // Get index_dir from environment variable first
    const char* env_index_dir = std::getenv("INDEX_DIR");
    if (env_index_dir && strlen(env_index_dir) > 0) {
        index_dir = env_index_dir;
    }
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--build-indexes") {
            build_indexes = true;
        } else if (arg.rfind("--", 0) == 0) {
            log("Unknown option: %s", arg.c_str());
            log("Usage: create_base_index [--build-indexes] [<index_dir>]");
            log("  index_dir can also be set via INDEX_DIR environment variable");
            return -1;
        } else {
            // Command line argument overrides environment variable
            index_dir = arg;
        }
    }
    
    // Validate required environment variables
    if (index_dir.empty()) {
        log("Error: INDEX_DIR environment variable not set and no index directory provided");
        log("Usage: create_base_index [--build-indexes] [<index_dir>]");
        return -1;
    }
    
    const char* db_connect = std::getenv("CANONICAL_MUSICBRAINZ_DATA_CONNECT");
    if (!db_connect || strlen(db_connect) == 0) {
        log("Error: CANONICAL_MUSICBRAINZ_DATA_CONNECT environment variable not set");
        return -1;
    }
    
    try {
        CreateBaseIndex importer(index_dir);
        importer.create();
        log("Mapping import completed successfully!");
        
        if (build_indexes) {
            // Build the search indexes
            log("Building artist indexes...");
            ArtistIndex *artist_index = new ArtistIndex(index_dir);
            artist_index->build();
            delete artist_index;
            
            log("Building recording indexes...");
            MBIDMapping mapping(index_dir);
            mapping.build_recording_indexes();
            
            log("All indexes built successfully!");
        }
    } catch (const std::exception& e) {
        log("Error: %s", e.what());
        return -1;
    }
    

    
    return 0;
}