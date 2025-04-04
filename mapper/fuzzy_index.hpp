#pragma once

#include <stdio.h>
#include <iostream>
#include <map>
#include <string>
#include <vector>
using namespace std;

#include "encode.hpp"

#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>

#include "index.h"
#include "init.h"
#include "index.h"
#include "params.h"
#include "rangequery.h"
#include "knnquery.h"
#include "knnqueue.h"
#include "methodfactory.h"
#include "spacefactory.h"
#include "space.h"
#include "space/space_vector.h"
#include "space/space_sparse_vector.h"
#include "knnquery.h"
#include "knnqueue.h"

const auto NUM_FUZZY_SEARCH_RESULTS = 500;

typedef jpcre2::select<char> jp; 

class IndexData {
    public:
        int id;
        string text;
        
        IndexData(int _id, const char *_text) {
            id = _id;
            text = _text;
        }
};

class IndexResult {
    public:
        int   id;
        float distance;
        
        IndexResult(int _id, float _distance) {
            id = _id;
            distance = _distance;
        }
};

class FuzzyIndex {
    private:
        vector<IndexData>        *index_data; 
        similarity::Index<float> *index = nullptr;
        similarity::Space<float> *space = nullptr;
     	TfIdfVectorizer           vectorizer;
        similarity::ObjectVector  vectorized_data;

    public:

        FuzzyIndex() :
     	    vectorizer(false, false) {
            index_data = new vector<IndexData>();

            similarity::initLibrary(0, LIB_LOGNONE, NULL);
            space = similarity::SpaceFactoryRegistry<float>::Instance().CreateSpace("negdotprod_sparse_fast",
                                                                                    similarity::AnyParams());
        }
        
        ~FuzzyIndex() {
            delete index_data;
            delete index;
            delete space;
        }

        void
        transform_text(const arma::mat &matrix, const vector<string> &text_data, similarity::ObjectVector &data) {
            std::vector<similarity::SparseVectElem<float>> sparse_items;            
            auto sparse_space = reinterpret_cast<const similarity::SpaceSparseVector<float>*>(space);

            for(int col = 0; col < matrix.n_cols; col++) {
                unsigned int index = 0;
                unsigned int row;
                for(row = 0; row < matrix.n_rows; row++) {
                    auto value = matrix(row,col);
                    if (value != 0.0) {
                        sparse_items.push_back(similarity::SparseVectElem<float>(index, value));
                    }
                    index++;
                }

                std::sort(sparse_items.begin(), sparse_items.end());
                data.push_back(sparse_space->CreateObjFromVect(col, -1, sparse_items));
                sparse_items.clear();
            }
        }
        
        void
        build(vector<IndexData> &index_data) {
            if (index_data.size() == 0)
                throw std::length_error("no index data provided.");
            
            vector<string> text_data;
            for(auto entry : index_data) 
                text_data.push_back(entry.text);

            arma::mat matrix = vectorizer.fit_transform(text_data);
            transform_text(matrix, text_data, vectorized_data);
            
            index = similarity::MethodFactoryRegistry<float>::Instance().CreateMethod(false,
                        "simple_invindx",
                        "negdotprod_sparse_fast",
                         *space,
                         vectorized_data);
            similarity::AnyParams index_params;
            index->CreateIndex(index_params);
        }

        vector<IndexResult> search(const string &query_string, float min_confidence, bool debug=false) {
            // Carry out search, returns list of dicts: "text", "id", "confidence" 
            vector<string> text_data;
            similarity::ObjectVector data;

            if (index == nullptr)
                throw std::length_error("no index available.");

            text_data.push_back(query_string);
            arma::mat matrix = vectorizer.transform(text_data);
            transform_text(matrix, text_data, data);

            unsigned k = NUM_FUZZY_SEARCH_RESULTS;
            similarity::KNNQuery<float> knn(*space, data[0], k);
            index->Search(&knn, -1);
            data.clear();
            
            vector<IndexResult> results;
            auto queue = knn.Result()->Clone();
            while (!queue->Empty()) {
                auto dist = -queue->TopDistance();
                if (dist > min_confidence)
                    results.push_back(IndexResult(queue->TopObject()->id(), dist));
                queue->Pop();
            }
            return results;
        }

        template<class Archive>
        void save(Archive & archive) const
        {
            vector<uint8_t> index_data;
            index->SerializeIndex(index_data, vectorized_data);

            archive(index_data, vectorizer); 
        }
      
        template<class Archive>
        void load(Archive & archive)
        {
            vector<uint8_t> index_data;
            // Clean up any object we may have
            for (auto datum : vectorized_data) {
                delete datum;
            }
            vectorized_data.clear();

            // Restore our data
            archive(index_data, vectorizer); 
            delete index;
    
            auto factory = similarity::MethodFactoryRegistry<float>::Instance();
            index = factory.CreateMethod(false, 
                                         "simple_invindx",
                                         "negdotprod_sparse_fast",
                                         *space, 
                                         vectorized_data);
            index->UnserializeIndex(index_data, vectorized_data);
        }
};