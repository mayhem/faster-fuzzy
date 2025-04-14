#pragma once

#include <cereal/archives/binary.hpp>

const auto MAX_ENCODED_STRING_LENGTH = 30;

class IndexSupplementalData {
    public:

        string       remainder;
        unsigned int artist_id;

        template<class Archive>
        void serialize(Archive & archive) const
        {
            archive(remainder, artist_id);
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
