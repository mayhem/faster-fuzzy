#pragma once

const auto MAX_ENCODED_STRING_LENGTH = 30;

class IndexResult {
    public:
        unsigned int id;
        float        distance;
        
        IndexResult(unsigned int _id, float _distance) {
            id = _id;
            distance = _distance;
        }
};
