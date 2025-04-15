#pragma once

#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>

const auto MAX_ENCODED_STRING_LENGTH = 30;

class EntityRef {
    public:
       unsigned int id;
       unsigned int rank;
       
        EntityRef(unsigned int id_, unsigned int rank_) {
            id = id_;
            rank = rank;
        }
        template<class Archive>
        void serialize(Archive & archive) const
        {
            archive(id, rank);
        }
};

class TempReleaseData {
    public:
       unsigned int       id;
       string             text, remainder;
       unsigned int       rank;
       
        TempReleaseData(unsigned int id_, string &text_, string &remainder_, unsigned int rank_) {
            id = id_;
            text = text_;
            remainder = remainder_;
            rank = rank_;
        }
};

class IndexSupplementalReleaseData {
    public:
       unsigned int       id;
       string             text_rem;
       vector<EntityRef> release_refs;
       
        IndexSupplementalReleaseData(unsigned int id_, const string &text_rem_, const vector<EntityRef> &refs_) {
            id = id_;
            text_rem = text_rem_;
            release_refs = refs_;
        }

        template<class Archive>
        void serialize(Archive & archive) const
        {
            archive(id, text_rem, release_refs);
        }
};

class IndexSupplementalData {
    public:

        string       remainder;
        unsigned int entity_id;

        template<class Archive>
        void serialize(Archive & archive) const
        {
            archive(remainder, entity_id);
        }
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
