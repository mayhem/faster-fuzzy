#pragma once

#include <stdio.h>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <math.h>
using namespace std;

#include "defs.hpp"
#include "tfidf_vectorizer.hpp"
#include "levenshtein.hpp"

#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>

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

const auto NUM_FUZZY_SEARCH_RESULTS = 10;

class FuzzyIndex {
    private:
        similarity::Index<float> *index = nullptr;
        similarity::Space<float> *space = nullptr;
     	TfIdfVectorizer           vectorizer;
        similarity::ObjectVector  vectorized_data;

    public:

        vector<unsigned int>      index_ids; 
        vector<string>            index_texts;   // the full text field, needed for matching long query strings

        FuzzyIndex() :
     	    vectorizer(false, false) {

            similarity::initLibrary(0, LIB_LOGNONE, NULL);
            space = similarity::SpaceFactoryRegistry<float>::Instance().CreateSpace("negdotprod_sparse_fast",
                                                                                    similarity::AnyParams());
        }
        
        ~FuzzyIndex() {
            for(auto &obj : vectorized_data)
                delete obj;
            delete index;
            delete space;
        }
        
        string
        get_index_text(unsigned int offset) {
            if (offset >= index_texts.size()) {
                printf("ERROR: get_index_text offset %u out of bounds (size: %zu)\n", offset, index_texts.size());
                return "";
            }
            return index_texts[offset];
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
            index_texts = text_data;
            vector<string> short_texts;
            for(auto & it : text_data)
                short_texts.push_back(it.substr(0, MAX_ENCODED_STRING_LENGTH));
           
            arma::sp_mat matrix = vectorizer.fit_transform(short_texts);
            transform_text(matrix, vectorized_data);
            
            index = similarity::MethodFactoryRegistry<float>::Instance().CreateMethod(false,
                        "simple_invindx",
                        "negdotprod_sparse_fast",
                         *space,
                         vectorized_data);
            similarity::AnyParams index_params;

            index->CreateIndex(index_params);
        }

        vector<IndexResult> *
        search(const string &query_string, float min_confidence, char source) {
            vector<string> text_data;
            similarity::ObjectVector data;
            vector<IndexResult> *results = new vector<IndexResult>;
            
            if (index == nullptr) {
                printf("No index available.");
                return nullptr;
            }

            text_data.push_back(query_string.substr(0, MAX_ENCODED_STRING_LENGTH));
            arma::sp_mat matrix = vectorizer.transform(text_data);
            transform_text(matrix, data);

            unsigned k = NUM_FUZZY_SEARCH_RESULTS;
            bool has_long = false;
            const unsigned max_k = 1000; // Reasonable upper limit to prevent infinite growth

            // Keep searching with increasing k until we get some non-perfect matches
            while (k <= max_k) {
                similarity::KNNQuery<float> knn(*space, data[0], k);
                index->Search(&knn, -1);

                bool found_non_perfect = false;
                results->clear(); // Clear previous results
                has_long = false; // Reset for each iteration
                
                auto queue = knn.Result()->Clone();
                while (!queue->Empty()) {
                    auto dist = -queue->TopDistance();
                    if (dist >= min_confidence) {
                        if (index_texts[queue->TopObject()->id()].size() > MAX_ENCODED_STRING_LENGTH)
                            has_long = true;
                        results->push_back(IndexResult(index_ids[queue->TopObject()->id()], queue->TopObject()->id(), dist, source));
                        
                        // Check if this result has confidence < 1.0
                        if (dist < 1.0) {
                            found_non_perfect = true;
                        }
                    }
                    queue->Pop();
                }
                delete queue;
                
                // If we found some non-perfect matches, no results at all, or we have results but 
                // didn't get any new results in this iteration (k exceeds index size), we're done
                if (found_non_perfect || results->empty() || results->size() < k) {
                    break;
                }
                
                // If all results are still perfect matches (1.0), double k and try again
                k += NUM_FUZZY_SEARCH_RESULTS;
            }
            for(auto &obj : data)
                delete obj;
            
            reverse(results->begin(), results->end());
            if (query_string.size() > MAX_ENCODED_STRING_LENGTH || has_long) {
                return post_process_long_query(query_string, results, min_confidence, source);
            }

            return results;
        }
         
        vector<IndexResult> *
        post_process_long_query(const string &query, vector<IndexResult> *results, float min_confidence, char source) {
            vector<IndexResult> *updated = new vector<IndexResult>;
            
            // Truncate query to max length to keep comparison consistent with index
            string truncated_query = query.substr(0, MAX_ENCODED_STRING_LENGTH);
          
            for(int i = results->size() - 1; i >= 0; i--) {
                unsigned int id = (*results)[i].id;
                unsigned int index = (*results)[i].result_index;
                // Also truncate index text for consistent comparison
                string truncated_index_text = index_texts[index].substr(0, MAX_ENCODED_STRING_LENGTH);
                size_t dist = lev_edit_distance(truncated_query.size(), (const lev_byte*)truncated_query.c_str(), 
                                                truncated_index_text.size(), (const lev_byte*)truncated_index_text.c_str(), 1);
                float conf;
                if (dist == 0)
                    conf = 1.0;
                else 
                    conf = 1.0 - fabs((float)dist / truncated_query.size());

                if (conf >= min_confidence) {
                    IndexResult temp = { id, index, conf, source };
                    updated->push_back(temp);
                }
            }
            return updated;
        }

        template<class Archive>
        void save(Archive & archive) const
        {
            vector<uint8_t> index_data;
            if (index)
                index->SerializeIndex(index_data, vectorized_data);
            archive(index_data, vectorizer, index_ids, index_texts); 
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
            archive(index_data, vectorizer, index_ids, index_texts); 
            delete index;
            
            
            if (index_data.size() == 0)
                return;
    
            auto factory = similarity::MethodFactoryRegistry<float>::Instance();
            index = factory.CreateMethod(false, 
                                         "simple_invindx",
                                         "negdotprod_sparse_fast",
                                         *space, 
                                         vectorized_data);
            index->UnserializeIndex(index_data, vectorized_data);
        }
};
