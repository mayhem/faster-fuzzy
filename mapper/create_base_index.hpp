#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <chrono>
#include <iostream>
#include <sstream>
#include <filesystem>

#include "SQLiteCpp.h"
#include "utils.hpp"

using namespace std;

const string DB_CONNECT = "dbname=musicbrainz_db user=musicbrainz host=localhost port=5432 password=musicbrainz";
const float ARTIST_CONFIDENCE_THRESHOLD = 0.45;
const int NUM_ROWS_PER_COMMIT = 25000;
const int MAX_THREADS = 8;

struct MappingRow {
    unsigned int artist_credit_id;
    string artist_mbids;
    string artist_credit_name;
    string artist_credit_sortname;
    unsigned int release_id;
    string release_mbid;
    unsigned int release_artist_credit_id;
    string release_name;
    unsigned int recording_id;
    string recording_mbid;
    string recording_name;
    unsigned int score;
};

class CreateBaseIndex {
private:
    string index_dir;
    void create_sqlite_db(const string& db_file);
    void import_csv_to_sqlite(const string& db_file, const string& csv_file);
    void create_indexes(const string& db_file);
    string escape_csv_field(const string& field);

public:
    CreateBaseIndex(const string& _index_dir);
    void create();
};