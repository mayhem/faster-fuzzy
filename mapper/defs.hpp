#pragma once

#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>

const auto MAX_ENCODED_STRING_LENGTH = 30;

class EntityRef {
    public:
       unsigned int id;
       unsigned int rank;
      
        EntityRef() {};
        EntityRef(unsigned int id_, unsigned int rank_) {
            id = id_;
            rank = rank_;
        }
        template<class Archive>
        void serialize(Archive & archive)
        {
            archive(id, rank);
        }
};

class TempReleaseData {
    public:
       unsigned int       id;
       string             text;
       unsigned int       rank;
       
        TempReleaseData(unsigned int id_, string &text_, unsigned int rank_) {
            id = id_;
            text = text_;
            rank = rank_;
        }
};

class IndexSupplementalReleaseData {
    public:
        vector<EntityRef> release_refs;
       
        IndexSupplementalReleaseData() {};
        IndexSupplementalReleaseData(const vector<EntityRef> &refs_) {
            release_refs = refs_;
        }
        
        void
        sort_refs_by_rank() {
            sort(release_refs.begin(), release_refs.end(), [](const EntityRef& a, const EntityRef& b) {
                return a.rank < b.rank;
            }); 
        }

        template<class Archive>
        void serialize(Archive & archive)
        {
            archive(release_refs);
        }
};

class FuzzyIndex;
class ArtistReleaseRecordingData {
    public:
        FuzzyIndex                           *recording_index, *release_index;
        vector<IndexSupplementalReleaseData> *release_data;

        ArtistReleaseRecordingData(FuzzyIndex *rec_index, FuzzyIndex *rel_index, 
                                   vector<IndexSupplementalReleaseData> *rel_data) {
            recording_index = rec_index;
            release_index = rel_index;
            release_data = rel_data;
        };
};

class IndexResult {
    public:
        unsigned int   id;
        unsigned int   result_index;
        // TODO: rename this to confidence
        float          confidence;
        
        IndexResult() {}
        IndexResult(unsigned int _id, unsigned int _result_index, float _confidence) {
            id = _id;
            confidence = _confidence;
            result_index = _result_index;
        }
};

class SearchResult {
    public:
        unsigned int   artist_credit_id, release_id, recording_id;
        float          confidence;
        string         artist_credit_name;
        vector<string> artist_credit_mbids;
        string         release_name;
        string         release_mbid;
        string         recording_name;
        string         recording_mbid;
       
        SearchResult() {
            artist_credit_id = 0;
            release_id = 0;
            recording_id = 0;
            confidence = 0.0;
        }
        SearchResult(unsigned int _artist_credit_id, unsigned int _release_id, unsigned int _recording_id, float _confidence) {
            artist_credit_id = _artist_credit_id;
            release_id = _release_id;
            recording_id = _recording_id;
            confidence = _confidence;
        };
};
