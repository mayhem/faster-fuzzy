#pragma once

#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/map.hpp>
#include <map>

const auto MAX_ENCODED_STRING_LENGTH = 30;

struct ReleaseRecordingLink {
    
    unsigned int release_index, release_id, rank;
    unsigned int recording_index, recording_id;

    template<class Archive>
    void serialize(Archive & archive)
    {
       archive(release_index, release_id, rank, recording_index, recording_id);
    }
};

class FuzzyIndex;
class ReleaseRecordingIndex {
    public:
        FuzzyIndex                                       *recording_index, *release_index;
        map<unsigned int, vector<ReleaseRecordingLink>>   links;

        ReleaseRecordingIndex(FuzzyIndex *rec_index,
                              FuzzyIndex *rel_index, 
                              map<unsigned int, vector<ReleaseRecordingLink>> &_links) {
            recording_index = rec_index;
            release_index = rel_index;
            links = _links;
        };

        ~ReleaseRecordingIndex();

        template<class Archive>
        void serialize(Archive & archive)
        {
           archive(links);
        }
};

class IndexResult {
    public:
        unsigned int   id;
        unsigned int   result_index;
        float          confidence;
        char           source;
        
        IndexResult(unsigned int _id, unsigned int _result_index, float _confidence, char _source) {
            id = _id;
            confidence = _confidence;
            result_index = _result_index;
            source = _source;
        }

        // Copy constructor
        IndexResult(const IndexResult& other) {
            id = other.id;
            result_index = other.result_index;
            confidence = other.confidence;
            source = other.source;
        }
};

class SearchMatch {
    public:
        unsigned int   artist_credit_id, release_id, recording_id;
        float          confidence;
        string         artist_credit_name;
        vector<string> artist_credit_mbids;
        string         release_name;
        string         release_mbid;
        string         recording_name;
        string         recording_mbid;
       
        SearchMatch() {
            artist_credit_id = 0;
            release_id = 0;
            recording_id = 0;
            confidence = 0.0;
        }
        SearchMatch(unsigned int _artist_credit_id, unsigned int _release_id, unsigned int _recording_id, float _confidence) {
            artist_credit_id = _artist_credit_id;
            release_id = _release_id;
            recording_id = _recording_id;
            confidence = _confidence;
        };

        SearchMatch(const SearchMatch& other) {
            artist_credit_id = other.artist_credit_id;
            release_id = other.release_id;
            recording_id = other.recording_id;
            confidence = other.confidence;
            artist_credit_name = other.artist_credit_name;
            artist_credit_mbids = other.artist_credit_mbids;
            release_name = other.release_name;
            release_mbid = other.release_mbid;
            recording_name = other.recording_name;
            recording_mbid = other.recording_mbid;
        }
};
