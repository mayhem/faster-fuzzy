#include <stdio.h>
#include <tuple>
#include <sstream>
#include <algorithm>
#include <iterator>
#include "fsm.hpp"
#include "test_cases.hpp"

#ifdef INFO
#undef INFO
#endif

#ifdef CHECK
#undef CHECK
#endif

#ifdef WARN
#undef WARN
#endif

#include <catch2/catch_all.hpp>

MappingSearch *mapping_search;

string
make_comma_sep_string(const vector<string> &str_array) {
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

string join(const vector<string>& strings, const string& delimiter) {
    if (strings.empty()) {
        return "";
    }
    ostringstream oss;
    copy(strings.begin(), strings.end() - 1,
              ostream_iterator<string>(oss, delimiter.c_str()));
    oss << strings.back();
    return oss.str();
}

tuple<string, string, string>
lookup(const string &artist_credit_name, const string &release_name, const string &recording_name) {
    SearchMatch *result = mapping_search->search(artist_credit_name, release_name, recording_name);
    if (!result) {
        tuple<string, string, string> ret = { string(), string(), string() };
        log("no matches");
        return ret; 
    }
    log("%-8d %s %s", 
        result->artist_credit_id,
        join(result->artist_credit_mbids, string(",")).c_str(),
        result->artist_credit_name.c_str());
    log("%-8d %s %s", 
        result->release_id,
        result->release_mbid.c_str(),
        result->release_name.c_str());
    log("%-8d %s %s\n", 
        result->recording_id,
        result->recording_mbid.c_str(),
        result->recording_name.c_str());
   
    string artist_mbids;
    int index = 0;
    for(auto &it : result->artist_credit_mbids) {
        if (index > 0)
            artist_mbids += string(",");
                
        artist_mbids += it;
        index += 1;
    }

    tuple<string, string, string> ret = { artist_mbids, result->release_mbid, result->recording_mbid };
    delete result;
    return ret;
}

TEST_CASE("basic lookup tests") {
    auto test_case = GENERATE(from_range(get_test_cases()));

    INFO("Artist: " << test_case.artist_credit_name);
    INFO("Release: " << test_case.release_name);
    INFO("Recording: " << test_case.recording_name);

    tuple<string, string, string> result = lookup(test_case.artist_credit_name,
                                                 test_case.release_name,
                                                 test_case.recording_name);

    REQUIRE(get<0>(result) == test_case.artist_credit_mbids);
    REQUIRE(get<1>(result) == test_case.release_mbid);
    REQUIRE(get<2>(result) == test_case.recording_mbid);
}

int main(int argc, char* argv[]) {
    init_logging();
    
    if (argc < 2) {
        log("Usage: mapping_tests <index_dir>");
        return -1;
    }
    
    string index_dir = string(argv[1]);
    ArtistIndex* artist_index = new ArtistIndex(index_dir);
    artist_index->load();
    IndexCache* index_cache = new IndexCache(10);
    
    mapping_search = new MappingSearch(index_dir, artist_index, index_cache);

    Catch::Session session;
    int returnCode = session.run(argc-1, argv+1);
    
    delete mapping_search;
    delete artist_index;
    delete index_cache;

    return returnCode;
}
