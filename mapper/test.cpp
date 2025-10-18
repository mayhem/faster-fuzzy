#include <stdio.h>
#include <tuple>
#include <sstream>
#include <algorithm>
#include <iterator>
#include "fsm.hpp"

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

FSMMappingSearch *mapping_search;

class TestCase {
    public:

        string artist_credit_name;
        string release_name;
        string recording_name;

        string artist_credit_mbids;
        string release_mbid;
        string recording_mbid;
};

// TODO: Artist credit ids in artist index are not unique

TestCase test_cases[] = {
    { "portishead", "portishead", "western eyes", "8f6bd1e4-fbe1-4f50-aa9b-94c450ec0f11", "10ac58ca-0655-4b16-a6cb-58fdc309de0a", "34745941-69c5-401a-bdb9-ae761a9b3562" },
    { "portished", "portishad", "western ey", "8f6bd1e4-fbe1-4f50-aa9b-94c450ec0f11", "10ac58ca-0655-4b16-a6cb-58fdc309de0a", "34745941-69c5-401a-bdb9-ae761a9b3562" },
    { "morcheeba","parts of the process","trigger hippie","067102ea-9519-4622-9077-57ca4164cfbb","1e5908d9-ffcd-3080-9920-ece4612a43c9","97e69767-5d34-4c97-b36a-f3b2b1ef9dae" },
    { "Billie Eilish","","COPYCAT","f4abc0b5-3f7a-4eff-8f78-ac078dbce533","37811723-1da9-404d-bd2c-a6a6352bdcc2", "ed3a357a-cd5c-4489-aeb3-25cc87bac005" },
    { "Angelo Badalamenti","Music From Twin Peaks","Laura Palmer’s Theme","5894dac5-0260-4175-b36e-e34680a859d6","e441d678-b225-3ea1-808c-9c488fdc3ac6","4cc6e566-96c3-4709-9ec0-a9b39c115e2f" },
    { "Alicia Keys","Keys","Skydive (originals)","8ef1df30-ae4f-4dbd-9351-1a32b208a01e", "f7c03888-7c31-4416-a57c-f1686f59ac89","a7bae269-2dd6-4297-a4c7-36a472a2a8fc" },
    { "Godspeed You! Black Emperor","Lift Your Skinny Fists Like Antennas to Heaven","Like Antennas To Heaven…","3648db01-b29d-4ab9-835c-83f6a5068fe4", "f77eeaef-878e-4db3-ae65-5161cf929f14", "5fb00c2b-5bc9-420a-882a-cc024d137a08" },
    { "Charli xcx","BRAT","365","260b6184-8828-48eb-945c-bc4cb6fc34ca","82082eb8-19a0-4957-9a99-e8fda1bb6b88","bd9fd6a1-d41b-4b82-9ead-a4f958749a77" },
    { "Milana","MALLORCA STONER VOL.1","Forest Tale","3ac26f6c-0370-4451-a321-0880849257f9", "60422eb6-5d26-4e8e-801a-ab0d30d787e4","3c532ced-12af-44f8-85d8-7e88302e4bbe" },
    { "x#!*┦97","1 LP","untitled 1","78308fa1-b583-4b6c-a592-931df0126d29","3e7cb8db-6d89-4a54-8128-113f297fa83d","bc3c4795-e1bf-4350-b976-09be082bf077" },
    { "Ishay Ribo", "סוף חמה לבוא","רבי שמעון","4d5abad8-d57b-4c3f-b477-043214e96dc4","677fc470-d38d-463b-a020-b82c2f25321e","b3068af6-aee0-4f37-b079-6f79cc9eabbd" },
    { "幾何学模様","masana temples","nana","3a605eba-b6a1-4298-855d-b3033df0bf8b","615b8e61-4be8-4385-a0d3-0894f91bfa6b","1bb37d6c-6eed-4294-8341-1efa4cdce0d8" },
    { "Kikagaku Moyo","masana temples","nana","3a605eba-b6a1-4298-855d-b3033df0bf8b","615b8e61-4be8-4385-a0d3-0894f91bfa6b","1bb37d6c-6eed-4294-8341-1efa4cdce0d8" },
    { "Harry Nilsson","Nilsson Schmilsson","Without You","e5963d26-01fa-40f5-b200-e0127f410a45","1ae84595-aa9d-3190-bce9-140eb81800e4","fa2013f9-dca8-4c1c-9481-753ffcadbae3" },
    { "xhashsymbolexclamationpointasteriskrightdoublevertical97","1 LP","untitled 1","78308fa1-b583-4b6c-a592-931df0126d29","3e7cb8db-6d89-4a54-8128-113f297fa83d","bc3c4795-e1bf-4350-b976-09be082bf077" },
    { "Enya Patricia Brennan", "watermark", "watermark", "4967c0a1-b9f3-465e-8440-4598fd9fc33c", "f65cccba-f726-445c-8d1f-e324e5ab779f", "6e686d11-0191-4021-9bb7-a46535709ca4"},
    { "Hanan Ben Ari","לא לבד","רגע","eced93df-6c3d-4f44-81a1-b7768bdaecf9","299f4e4e-b553-44ba-8f8b-199a17eb9038","2ca43050-1bff-4cda-b07e-732d8f6727ba" },
    { "!!!","As If","Ooo","f26c72d3-e52c-467b-b651-679c73d8e1a7","a8d39759-5f8f-4e64-bf53-93e11af9d159","f2233161-cf68-421e-bc97-de8dbec3fc3c" },
    { "Yaakov Shwekey","גוף ונשמה 1/4","גלגלים","b5792b6a-1561-4499-a9d3-82fa7f40b33f", "90820c40-dfee-4d31-9147-a56383df402b","59855bd1-9e13-4b55-9c77-c793c4dd8cc2" },
    { "mynoise", "primeval forest", "springtime birds", "d6ecdfbe-aaa7-48b5-925c-87f066376eaa", "00d9a760-bf8a-4c9e-b310-f7e00b7ed701", "057c48e8-754c-487e-b46b-f55a76498ccd"},
    { "queen & david bowie","Hot Space","under pressure","0383dadf-2a4e-4d10-a46a-e9e041da8eb3,5441c29d-3602-4898-b1a1-b77fa23b8e50","b4f42dd2-c6cc-449d-84ee-de581dcf120e","32c7e292-14f1-4080-bddf-ef852e0a4c59" },
    { "queen","Hot Space","under pressure","0383dadf-2a4e-4d10-a46a-e9e041da8eb3,5441c29d-3602-4898-b1a1-b77fa23b8e50","b4f42dd2-c6cc-449d-84ee-de581dcf120e","32c7e292-14f1-4080-bddf-ef852e0a4c59" },
    { "darkseed","","entre dos tierras","8b0ab1c4-ffe4-491e-adda-037c744d1b00","","c7ba26e3-66c9-40a7-af72-cd27cdeed09c" },
    { "guns n' roses","Appetite for Destruction","welcome to the jungle","eeb1195b-f213-4ce1-b28c-8565211f8e43","2426fb8e-47fa-416c-bdc5-72139263f99e","e753cdd1-6e64-4879-8a40-c46744a897b7" },
    { "guns n' roses","Appetite for Destruction","mr. brownstone","eeb1195b-f213-4ce1-b28c-8565211f8e43","2426fb8e-47fa-416c-bdc5-72139263f99e","efcefd95-3b47-490f-b506-f3adf97fba55" },
    { "pink floyd","Animals","pigs on the wing part 1","83d91898-7763-47d7-b03b-b92132375c47","e802a957-519f-3382-a9cb-a8bb2d0be466","aca2620e-eee7-416c-bb3b-b881b7d68780" },
    { "nine inch nails","The Downward Spiral","hurt","b7ffd2af-418f-4be2-bdd1-22f8b48613da","ba8701ba-dc7c-4bca-9c83-846ee8c3d576","ab7805a8-c161-403d-92bf-a92c8b8e17dc" },
    { "portishead","Dummy","glory box","8f6bd1e4-fbe1-4f50-aa9b-94c450ec0f11","76df3287-6cda-33eb-8e9a-044b5e15ffdd","145f5c43-0ac2-4886-8b09-63d0e92ded5d" },
    { "thievery corporation","The Richest Man in Babylon","heaven's gonna burn your eyes","a505bb48-ad65-4af4-ae47-29149715bff9","b2a820cc-c0ad-4aa3-a2a7-ed42ead88017","fac75e6d-95e4-47b7-b469-5662ef15d3de" },
    { "telepopmusik","Genetic World","trishika","265f242e-cf4e-4fbe-a3fe-43112387172f","dcdc934a-4f90-3306-a9aa-bd874c404062","ba9df4e1-fe39-49d0-80ae-43bf70c175c8" },
    { "charli xcx","SUCKER","break the rules","260b6184-8828-48eb-945c-bc4cb6fc34ca","b0ea48e1-5b5f-4b7b-8e3e-c6d7fb2abe43","8a0add0c-c733-4a72-9bc7-85fd5322865b" },
    { "charli xcx","SUCKER","break the rules (femme remix)","260b6184-8828-48eb-945c-bc4cb6fc34ca","b0ea48e1-5b5f-4b7b-8e3e-c6d7fb2abe43","fbec0a7a-bdd0-4379-b6a0-ecdc62f76a48" },
    { "daft punk","Random Access Memories","horizon","056e4f3e-d505-4dad-8ec1-d04f521cbb56","79215cdf-4764-4dee-b0b9-fec1643df7c5","befed7fb-a77e-49a9-8005-c7e36d5173cf" },
    { "daft punk","","horizon","056e4f3e-d505-4dad-8ec1-d04f521cbb56","79215cdf-4764-4dee-b0b9-fec1643df7c5","befed7fb-a77e-49a9-8005-c7e36d5173cf" },
    { "the xx","Coexist","reconsider","c5c2ea1c-4bde-4f4d-bd0b-47b200bf99d6","c58f53f5-a071-48fd-bb6b-8f344b2eca34","324d0034-9648-4f09-821e-1ef658f1c747" },
    { "florence + the machine","Lungs","bird song","5fee3020-513b-48c2-b1f7-4681b01db0c6","f4a3ef7e-ad82-3eba-8f20-425536925309","99d62964-f40a-466c-9293-3c5cae72f8ad" },
    { "the libertines","The Libertines","cyclops","82b304c0-7da4-45d3-896a-0767c7ae1141","d224298c-82a1-4668-a3d3-c3bd40c8fbb1","c23e0d75-e3a2-43f8-8b76-ae31d8e7eb31" },
    { "foo fighters","Echoes, Silence, Patience & Grace","seda","67f66c07-6e61-4026-ade5-7e782fad3a5d","9bb88ca5-583a-4e21-96e8-994a5fc900e5","2535f4e5-7760-4740-8458-8176b09718ce" },
    { "the libertines","The Libertines","cyclops","82b304c0-7da4-45d3-896a-0767c7ae1141","d224298c-82a1-4668-a3d3-c3bd40c8fbb1","c23e0d75-e3a2-43f8-8b76-ae31d8e7eb31" },
    { "lorde","Pure Heroine","bravado (fffrrannno remix)","8e494408-8620-4c6a-82c2-c2ca4a1e4f12","491dd8ee-2b66-4510-b457-919b39058fe6","a9e44959-d060-4f47-a434-1db824c11ed5" },
    { "queen","A Day at the Races","teo torriatte (let us cling together)","0383dadf-2a4e-4d10-a46a-e9e041da8eb3","2183a8fd-569e-440d-9a8d-c6eeaa589c71","8317a338-cfb4-46e6-956b-4e6495feebbf" },
    { "lana del rey","Ultraviolence","flipside","b7539c32-53e7-4908-bda3-81449c367da6","abc92375-0d33-4479-aba0-97abd12f579e","b5a37a27-ea68-4adf-952a-4679c306e344" },
    { "david bowie","The Next Day","god bless the girl","5441c29d-3602-4898-b1a1-b77fa23b8e50","6241d76e-45d9-4f7e-b9fc-24ad33e4955a","68cbaf8d-7e6b-40e7-a404-8367935ea29f" },
    { "arctic monkeys","AM","2013","ada7a83c-e3e1-40f1-93f9-3e73dbc9298a","a432408f-335e-4d64-b33a-c1fb809af58f","e5bdbeac-6e52-495c-a51b-6b834aaccfc8" },
    { "void", "", "verdict for worst dictator", "c8a0cb7a-99c8-4bcd-82d5-bcef201e13d1", "5941ccda-d6b5-4d5e-8a1a-5ca755321482", "ac02d630-b95d-4942-87e7-2b8cc11debf6" },
    { "鬱P feat. 初音ミク","HAPPYPILLS","ガ","3fdaac99-0261-49a7-92b3-5a909cba187d,130d679a-9a92-4373-8348-0800b6b93a30","df379666-f1de-4789-a8e8-c429a89dd1d2","6fd3bc0e-170f-45e6-a3c8-91943fd61c9c" },
    { "void", "Remind a Locus", "verdict for worst dictator", "c8a0cb7a-99c8-4bcd-82d5-bcef201e13d1", "5941ccda-d6b5-4d5e-8a1a-5ca755321482", "ac02d630-b95d-4942-87e7-2b8cc11debf6" },
    { "void", "", "verdict for worst dictator", "c8a0cb7a-99c8-4bcd-82d5-bcef201e13d1", "5941ccda-d6b5-4d5e-8a1a-5ca755321482", "ac02d630-b95d-4942-87e7-2b8cc11debf6" },
    { "鬱P","HAPPYPILLS","ガ","3fdaac99-0261-49a7-92b3-5a909cba187d,130d679a-9a92-4373-8348-0800b6b93a30","df379666-f1de-4789-a8e8-c429a89dd1d2","6fd3bc0e-170f-45e6-a3c8-91943fd61c9c" },


// s 鬱P, happypills, ガ
// release in:  1065353
// recording, "ga" 2063311
// 
// AC's: 1065353,3091565,3258059

// Possible recording/release match issue. requires more research
//    { "Celtic Woman","20th Anniversary","When You Believe","4d483147-c871-48c4-8470-85e5a66381c5","9659808f-3382-42be-8c5d-477d271f9791","376e5743-7bf5-49ee-9c7e-f00aa479882c" },
// Album/recording matching bug . nor research
//    { "Alicia Keys","Keys","Skydive","8ef1df30-ae4f-4dbd-9351-1a32b208a01e","f7c03888-7c31-4416-a57c-f1686f59ac89","a7bae269-2dd6-4297-a4c7-36a472a2a8fc" },
//
// This one requires Album + Soundtrack to be ranked higher than EP.
//    { "Angelo Badalamenti","","Laura Palmer’s Theme","e441d678-b225-3ea1-808c-9c488fdc3ac6","5894dac5-0260-4175-b36e-e34680a859d6","4cc6e566-96c3-4709-9ec0-a9b39c115e2f" },
//    { "The Beach Boys","The Smile Sessions","Child Is Father of the Man","ebfc1398-8d96-47e3-82c3-f782abcdb13d","581db767-5221-424b-b6a0-d5db2ff707a1","710ef859-ad3e-4e0e-981b-cecad50e41f4" },
// Needs stupid indexes for release and recordings!
//    { "!!!","!!!","KooKooKa Fuk‐U","f26c72d3-e52c-467b-b651-679c73d8e1a7","c4d9a024-c5d7-40c4-928d-0e3873cc7228","5c811d80-2743-461a-a163-82e14382aad7" },
// Detune -- single
//    { "Eve","pray - Single","pray","66bdd1c9-d1c5-40b7-a487-5061fffbd87d","9117d976-7283-4517-b5ac-513e62009613","f8c50031-b2e0-4b60-b8f6-38215271092c" },
// Detune, Artist credit for track is quite different. Rmoving all after "," will get the right match
//    { "Hans Zimmer, Lorne Balfe & Benjamin Wallfisch","Dunkirk: Original Motion Picture Soundtrack","End Titles","9cba57da-0a50-48d0-8a7b-232e31e196a0", "4d4b8a77-6c2e-4e6f-bc12-e563e9efab91","94f64a3f-b7d8-472f-99bb-ca07afad55da" },
// Data issues
//    -- Ornette is not album by her, but her quartet. No aliases exist. Probably consider this a data issue.
//    { "Ornette Coleman","Ornette!","W.R.U.","31ea99e3-c222-4809-8912-95678314beec","4c7b347e-16b2-41eb-b56e-97edb77ee961","43065996-51e7-4942-8803-aa2a0249b8a6" },
//    -- Galgalim is the translation of the hebrew word, we're only searching for transliterations.
//    { "Yaakov Shwekey","גוף ונשמה 1/4","Galgalim","b5792b6a-1561-4499-a9d3-82fa7f40b33f", "90820c40-dfee-4d31-9147-a56383df402b","59855bd1-9e13-4b55-9c77-c793c4dd8cc2" },
//    
// Test cases from old mapper:
// richard wagner,die walkure: feuerzauber,1756c795-9c60-4052-8aae-a4847c5a243c
//erosion 89,rising from the ashes,6979fc1a-f6bc-45a6-9240-a0ca06d213b3
//linkin park,1Stp klosr,f662175c-070f-4d93-b85b-0068aeaacdfc
//linkin park,Ppr:kut,f662175c-070f-4d93-b85b-0068aeaacdfc
//jonah matranga,new york,9a9a1473-d4a6-41c8-bee8-ded086677d18
//halou,the professional,ab5301c5-4ab9-4c71-81fc-96cc3faf9de4
//agents of satan,h.o.g.,ec195e5e-344e-4c24-adec-3c69f0330c18
//river city tanlines,gimme whatever,88c55540-0a0a-4cc3-8e39-2cadd68868e8
//bob dylan,turkey chase,c5f460cd-4564-40d9-b684-0d499429d0a0
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
    SearchMatches *result = mapping_search->search(artist_credit_name, release_name, recording_name);
    if (!result) {
        tuple<string, string, string> ret = { string(), string(), string() };
        printf("no matches\n");
        return ret; 
    }
    printf("%-8d %s %s\n", 
        result->artist_credit_id,
        join(result->artist_credit_mbids, string(",")).c_str(),
        result->artist_credit_name.c_str());
    printf("%-8d %s %s\n", 
        result->release_id,
        result->release_mbid.c_str(),
        result->release_name.c_str());
    printf("%-8d %s %s\n\n", 
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

vector<TestCase> get_test_cases() {
    vector<TestCase> vec(std::begin(test_cases), std::end(test_cases));
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
    
    mapping_search = new FSMMappingSearch(string(argv[1]), 10);
    mapping_search->load();

    Catch::Session session;
    int returnCode = session.run(argc-1, argv+1);
    
    delete mapping_search;

    return returnCode;
}
