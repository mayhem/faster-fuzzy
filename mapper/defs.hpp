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
        EntityRef(const unsigned int id_, const unsigned int rank_) {
            id = id_;
            rank = rank_;
        }
        template<class Archive>
        void serialize(Archive & archive)
        {
            archive(id, rank);
        }
};

// Get rid of this?
class IndexSupplementalReleaseData {
    public:
       vector<EntityRef> release_refs;
       
        IndexSupplementalReleaseData() {};
        IndexSupplementalReleaseData(const vector<EntityRef> &refs_) {
            release_refs = refs_;
        }

        template<class Archive>
        void serialize(Archive & archive)
        {
            archive(release_refs);
        }
};

class TempReleaseData {
    public:
       unsigned int       id;
       string             text, remainder;
       unsigned int       rank;
       
        TempReleaseData(unsigned int id_, string &text_, unsigned int rank_) {
            id = id_;
            text = text_;
            rank = rank_;
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
        unsigned int id;
        float        distance;
        
        IndexResult(unsigned int _id, float _distance) {
            id = _id;
            distance = _distance;
        }
};
