#pragma once

#include <stdio.h>
#include <iostream>
#include <map>
#include <string>
#include <vector>
using namespace std;

#include "tfidf_vectorizer.hpp"
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

class FuzzyIndex {
    private:
        vector<unsigned int>      index_ids; 
        similarity::Index<float> *index = nullptr;
        similarity::Space<float> *space = nullptr;
     	TfIdfVectorizer           vectorizer;
        similarity::ObjectVector  vectorized_data;
        EncodeSearchData          encode;

    public:

        FuzzyIndex() :
     	    vectorizer(false, false) {

            similarity::initLibrary(0, LIB_LOGNONE, NULL);
            space = similarity::SpaceFactoryRegistry<float>::Instance().CreateSpace("negdotprod_sparse_fast",
                                                                                    similarity::AnyParams());
        }
        
        ~FuzzyIndex() {
            delete index;
            delete space;
        }

        void
        transform_text(const arma::sp_mat &matrix, similarity::ObjectVector &data) {
            std::vector<similarity::SparseVectElem<float>> sparse_items;            
            auto sparse_space = reinterpret_cast<const similarity::SpaceSparseVector<float>*>(space);
           
            arma::uword last_col = 0;
            for(arma::sp_mat::const_iterator it = matrix.begin(); it != matrix.end(); ++it) {
                if (it.col() != last_col) {
                    std::sort(sparse_items.begin(), sparse_items.end());
                    data.push_back(sparse_space->CreateObjFromVect(it.col()-1, -1, sparse_items));
                    sparse_items.clear();
                }
                sparse_items.push_back(similarity::SparseVectElem<float>(it.row(), *it));
                last_col = it.col();
            }
            std::sort(sparse_items.begin(), sparse_items.end());
            data.push_back(sparse_space->CreateObjFromVect(last_col, -1, sparse_items));
            sparse_items.clear();
        }

        void
        build(vector<unsigned int> &_index_ids, vector<string> &text_data) {
            
            if (text_data.size() == 0)
                throw std::length_error("no index data provided.");
            if (text_data.size() != _index_ids.size())
                throw std::length_error("Length of ids and text vectors differs!");

            // Make a copy, I hope, of the index id data and hold on to it
            index_ids = _index_ids; 
      
            arma::sp_mat matrix = vectorizer.fit_transform(text_data);
            transform_text(matrix, vectorized_data);
            
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
            arma::sp_mat matrix = vectorizer.transform(text_data);
            transform_text(matrix, data);

            unsigned k = NUM_FUZZY_SEARCH_RESULTS;
            similarity::KNNQuery<float> knn(*space, data[0], k);
            index->Search(&knn, -1);
            data.clear();
            
            vector<IndexResult> results;
            auto queue = knn.Result()->Clone();
            while (!queue->Empty()) {
                auto dist = -queue->TopDistance();
                if (dist > min_confidence)
                    results.push_back(IndexResult(index_ids[queue->TopObject()->id()], dist));
                queue->Pop();
            }
            return results;
        }

        template<class Archive>
        void save(Archive & archive) const
        {
            vector<uint8_t> index_data;
            index->SerializeIndex(index_data, vectorized_data);

            archive(index_data, vectorizer, index_ids); 
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
            archive(index_data, vectorizer, index_ids); 
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