#pragma once

#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>

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
       string             text_rem;
       vector<EntityRef> release_refs;
       
        IndexSupplementalReleaseData() {};
        IndexSupplementalReleaseData(const string &text_rem_, const vector<EntityRef> &refs_) {
            text_rem = text_rem_;
            release_refs = refs_;
        }

        template<class Archive>
        void serialize(Archive & archive)
        {
            archive(text_rem, release_refs);
        }
};

class IndexSupplementalData {
    public:

        string       text_rem;

        IndexSupplementalData() {};
        IndexSupplementalData(const string &text_rem_) {
            text_rem = text_rem_;
        }

        template<class Archive>
        void serialize(Archive & archive)
        {
            archive(text_rem);
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
