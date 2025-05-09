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
    { "Billie Eilish","","COPYCAT","11e9b34f-eef3-4487-8fcc-57658638cb66","ed3a357a-cd5c-4489-aeb3-25cc87bac005" },
    { "Benjamin Wallfisch, Lorne Balfe and Hans Zimmer","Dunkirk: Original Motion Picture Soundtrack","End Titles","4d4b8a77-6c2e-4e6f-bc12-e563e9efab91","94f64a3f-b7d8-472f-99bb-ca07afad55da" },
    { "Hans Zimmer, Lorne Balfe & Benjamin Wallfisch","Dunkirk: Original Motion Picture Soundtrack","End Titles","4d4b8a77-6c2e-4e6f-bc12-e563e9efab91","94f64a3f-b7d8-472f-99bb-ca07afad55da" },
    { "Angelo Badalamenti","Music From Twin Peaks","Laura Palmer’s Theme","e441d678-b225-3ea1-808c-9c488fdc3ac6","4cc6e566-96c3-4709-9ec0-a9b39c115e2f" },
    { "Angelo Badalamenti","","Laura Palmer’s Theme","e441d678-b225-3ea1-808c-9c488fdc3ac6","4cc6e566-96c3-4709-9ec0-a9b39c115e2f" },
    { "Ishay Ribo","סוף חמה לבוא","רבי שמעון","677fc470-d38d-463b-a020-b82c2f25321e","b3068af6-aee0-4f37-b079-6f79cc9eabbd" },
    { "Yaakov Shwekey","גוף ונשמה 1/4","Galgalim","90820c40-dfee-4d31-9147-a56383df402b","59855bd1-9e13-4b55-9c77-c793c4dd8cc2" },
    { "Hanan Ben Ari","לא לבד","רגע","299f4e4e-b553-44ba-8f8b-199a17eb9038","2ca43050-1bff-4cda-b07e-732d8f6727ba" },
    { "Alicia Keys","Keys","Skydive (originals)","f7c03888-7c31-4416-a57c-f1686f59ac89","a7bae269-2dd6-4297-a4c7-36a472a2a8fc" },
    { "Alicia Keys","Keys","Skydive","f7c03888-7c31-4416-a57c-f1686f59ac89","a7bae269-2dd6-4297-a4c7-36a472a2a8fc" },
    { "Godspeed You! Black Emperor","Lift Your Skinny Fists Like Antennas to Heaven","Like Antennas To Heaven…","f77eeaef-878e-4db3-ae65-5161cf929f14","5fb00c2b-5bc9-420a-882a-cc024d137a08" },
    { "The Beach Boys","The Smile Sessions","Child Is Father of the Man","581db767-5221-424b-b6a0-d5db2ff707a1","710ef859-ad3e-4e0e-981b-cecad50e41f4" },
    { "Eve","pray - Single","pray","9117d976-7283-4517-b5ac-513e62009613","f8c50031-b2e0-4b60-b8f6-38215271092c" },
    { "Celtic Woman","20th Anniversary","When You Believe","9659808f-3382-42be-8c5d-477d271f9791","376e5743-7bf5-49ee-9c7e-f00aa479882c" },
    { "!!!","!!!","KooKooKa Fuk‐U","c4d9a024-c5d7-40c4-928d-0e3873cc7228","5c811d80-2743-461a-a163-82e14382aad7" },
    { "!!!","As If","Ooo","a8d39759-5f8f-4e64-bf53-93e11af9d159","f2233161-cf68-421e-bc97-de8dbec3fc3c" },
    { "Charli xcx","BRAT","365","9f7a7091-22b3-4fc7-b9aa-ebbed8ca5518","bd9fd6a1-d41b-4b82-9ead-a4f958749a77" },
    { "Milana","MALLORCA STONER VOL.1","Forest Tale","60422eb6-5d26-4e8e-801a-ab0d30d787e4","3c532ced-12af-44f8-85d8-7e88302e4bbe" },
    { "xhashsymbolexclamationpointasteriskrightdoublevertical97","1 LP","untitled 1","3e7cb8db-6d89-4a54-8128-113f297fa83d","bc3c4795-e1bf-4350-b976-09be082bf077" },
    { "Harry Nilsson","Nilsson Schmilsson","Without You","9be5bdcb-692b-4d7e-8d16-6d5741539ade","fa2013f9-dca8-4c1c-9481-753ffcadbae3" },
    { "Ornette Coleman","Ornette!","W.R.U.","4c7b347e-16b2-41eb-b56e-97edb77ee961","43065996-51e7-4942-8803-aa2a0249b8a6" },
    
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