#include <stdio.h>
#include <tuple>
#include "search.hpp"

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

class TestCase {
    public:

        string artist_credit_name;
        string release_name;
        string recording_name;

        string artist_credit_mbids;
        string release_mbid;
        string recording_mbid;
};

TestCase test_cases[] = {
    { "portishead", "portishead", "western eyes", "8f6bd1e4-fbe1-4f50-aa9b-94c450ec0f11", "10ac58ca-0655-4b16-a6cb-58fdc309de0a", "34745941-69c5-401a-bdb9-ae761a9b3562" },
    { "morcheeba","parts of the process","trigger hippie","067102ea-9519-4622-9077-57ca4164cfbb","1e5908d9-ffcd-3080-9920-ece4612a43c9","97e69767-5d34-4c97-b36a-f3b2b1ef9dae" },
    

    
    // Leave this sentinel here for , sake
    { "portished", "portishad", "western ey", "8f6bd1e4-fbe1-4f50-aa9b-94c450ec0f11", "10ac58ca-0655-4b16-a6cb-58fdc309de0a", "34745941-69c5-401a-bdb9-ae761a9b3562" }
};

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

tuple<string, string, string>
lookup(const string &artist_credit_name, const string &release_name, const string &recording_name) {
    SearchResult *result = mapping_search->search(artist_credit_name, release_name, recording_name);
   
    string artist_mbids;
    int index = 0;
    for(auto &it : result->artist_credit_mbids) {
        if (index > 0)
            artist_mbids += string(",");
                
        artist_mbids += it;
        index += 1;
    }

    tuple<string, string, string> ret = { artist_mbids, result->release_mbid, result->recording_mbid };
    return ret;
}

vector<TestCase> get_test_cases() {
    vector<TestCase> vec(begin(test_cases), end(test_cases));
    return vec;
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

    if (argc < 2) {
        printf("Usage: mapping_tests <index_dir>\n");
        return -1;
    }
    
    mapping_search = new MappingSearch(string(argv[1]), 10);
    mapping_search->load();

    Catch::Session session;
    int returnCode = session.run(argc-1, argv+1);
    
    delete mapping_search;

    return returnCode;
}