#include <stdio.h>

#include <iostream>
#include <map>
#include <string>
#include <vector>
//#include <bits/stdc++.h>
using namespace std;

#include "jpcre2.hpp"
#include "unidecode/unidecode.hpp"
#include "unidecode/utf8_string_iterator.hpp"
#include "tfidf_vectorizer.hpp"

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

const auto MAX_ENCODED_STRING_LENGTH = 30;
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
        string                    name;
        vector<IndexData>        *index_data; 
        similarity::Index<float> *index = nullptr;
        similarity::Space<float> *space = nullptr;
     	TfIdfVectorizer          *vectorizer = nullptr;
        
        jp::Regex                *non_word;
        jp::Regex                *spaces_uscore;
        jp::Regex                *spaces;

    public:

        FuzzyIndex(const string &_name) {
            index_data = new vector<IndexData>();
            string name = _name;
            non_word = new jp::Regex();
            spaces_uscore = new jp::Regex();
            spaces = new jp::Regex();
            vectorizer = new TfIdfVectorizer();

            non_word->setPattern("[^\\w]+").addModifier("n").compile();
            spaces_uscore->setPattern("[ _]+").addModifier("n").compile();
            spaces->setPattern("[\\s]+").addModifier("n").compile();
            
            similarity::initLibrary(0, LIB_LOGNONE, NULL);
        }
        
        ~FuzzyIndex() {
            delete vectorizer;
            delete index_data;
            delete non_word;
            delete spaces;
            delete index;
            if (space != nullptr)
                delete space;
        }

        string unidecode(const string &str) {
            unidecode::Utf8StringIterator begin = str.c_str();
            unidecode::Utf8StringIterator end = str.c_str() + str.length();
            string output;
            unidecode::Unidecode(begin, end, std::back_inserter(output));
            return output;
        }

        vector<string> encode_string(const string &text) {
            // Remove spaces, punctuation, convert non-ascii characters to some romanized equivalent, lower case, return
            if (text.empty()) {
                vector<string> a;
                return a;
            }
            string cleaned(non_word->replace(text,"", "g"));
            cleaned = unidecode(cleaned);
            transform(cleaned.begin(), cleaned.end(), cleaned.begin(), ::tolower);
            // Sometimes unidecode puts spaces in, so remove them
            string ret(spaces_uscore->replace(cleaned,"", "g"));
        
            auto main_part = ret.substr(0, MAX_ENCODED_STRING_LENGTH);
            string remainder;
            if (ret.length() > MAX_ENCODED_STRING_LENGTH)
                remainder = ret.substr(MAX_ENCODED_STRING_LENGTH);
            
            vector<string> out = { main_part, remainder};
            return out;
        }

        vector<string>
        encode_string_for_stupid_artists(const string &text) {
            //Remove spaces, convert non-ascii characters to some romanized equivalent, lower case, return
            if (text.empty()) {
                vector<string> a;
                return a;
            }

            string cleaned(spaces->replace(text,"", "g"));
            transform(cleaned.begin(), cleaned.end(), cleaned.begin(), ::tolower);
        
            auto main_part = cleaned.substr(0, MAX_ENCODED_STRING_LENGTH);
            string remainder;
            if (cleaned.length() > MAX_ENCODED_STRING_LENGTH)
                remainder = cleaned.substr(MAX_ENCODED_STRING_LENGTH);
            
            vector<string> out = { main_part, remainder};
            return out;
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

            space = similarity::SpaceFactoryRegistry<float>::Instance().CreateSpace("negdotprod_sparse_fast",
                                                                                    similarity::AnyParams());
            
            vector<string> text_data;
            for(auto entry : index_data) 
                text_data.push_back(entry.text);

            arma::mat matrix = vectorizer->fit_transform(text_data);
            similarity::ObjectVector *data = new similarity::ObjectVector();
            transform_text(matrix, text_data, *data);
            
            index = similarity::MethodFactoryRegistry<float>::Instance().CreateMethod(true,
                        "simple_invindx",
                        "negdotprod_sparse_fast",
                         *space,
                         *data);
            similarity::AnyParams index_params;
            index->CreateIndex(index_params);
        }

        vector<IndexResult> search(const string &query_string, float min_confidence, bool debug=false) {
            // Carry out search, returns list of dicts: "text", "id", "confidence" 

            if (index == nullptr)
                throw std::length_error("no index available.");

            vector<string> text_data;
            text_data.push_back(query_string);
            arma::mat matrix = vectorizer->transform(text_data);
            similarity::ObjectVector data;
            transform_text(matrix, text_data, data);

            unsigned k = NUM_FUZZY_SEARCH_RESULTS;
            similarity::KNNQuery<float> knn(*space, data[0], k);
            index->Search(&knn, -1);
            data.clear();
            
            cout << knn.Result()->Empty() <<  endl; 
            vector<IndexResult> results;
            auto queue = knn.Result()->Clone();
            while (!queue->Empty()) {
                auto dist = queue->TopDistance();
                // TODO: re-enable after debugging
                //if (dist > min_confidence)
                results.push_back(IndexResult(queue->TopObject()->id(), dist));
                queue->Pop();
            }
            return results;
        }

        void save(string &index_dir) {
//            v_file = os.path.join(index_dir, "%s_nmslib_vectorizer.pickle" % self.name)
//            i_file = os.path.join(index_dir, "%s_nmslib_index.pickle" % self.name)
//            d_file = os.path.join(index_dir, "%s_additional_index_data.pickle" % self.name)

//            with open(v_file, "wb") as f:
//                pickle.dump(self.vectorizer, f)
//            self.index.saveIndex(i_file, save_data=True)
//            with open(d_file, "wb") as f:
//                pickle.dump(self.index_data, f)
        }
                
        void save_to_mem(string &temp_dir) {
//            # TODO: Use scikit to pickle the vectorizer for better memory performance
//            # TODO: Do not save the additional data, its only used for debugging
//            vec = pickle.dumps(self.vectorizer)
//            additional_data = pickle.dumps(self.index_data)
//
//            i_file = os.path.join(temp_dir, "%d.pickle" % os.getpid())
//            self.index.saveIndex(i_file, save_data=True)
//            with open(i_file, "rb") as f:
//                index = f.read()
//            os.unlink(i_file)
//            
//            i_dat_file = os.path.join(temp_dir, "%d.pickle.dat" % os.getpid())
//            with open(i_dat_file, "rb") as f:
//                index_data = f.read()
//            os.unlink(i_dat_file)
//
//            # Ready for pickling
//            return { "vec": vec,
//                     "index": index,
//                     "index_data": index_data,
//                     "additional_data": additional_data }
        }

        void load(string &index_dir) {
//            v_file = os.path.join(index_dir, "%s_nmslib_vectorizer.pickle" % self.name)
//            i_file = os.path.join(index_dir, "%s_nmslib_index.pickle" % self.name)
//            d_file = os.path.join(index_dir, "%s_additional_index_data.pickle" % self.name)
//
//            try:
//                with open(v_file, "rb") as f:
//                    self.vectorizer = pickle.load(f)
//                self.index.loadIndex(i_file, load_data=True)
//                with open(d_file, "rb") as f:
//                    self.index_data = pickle.load(f)
//                return True
 //           except OSError:
//                return False
        }

        void load_from_mem(IndexData &data, string &temp_dir) {
//            self.vectorizer = pickle.loads(data["vec"])
//            self.index_data = pickle.loads(data["additional_data"])
//
//            i_file = os.path.join(temp_dir, "%d.pickle" % os.getpid())
//            with open(i_file, "wb") as f:
//                f.write(data["index"])
//            i_dat_file = os.path.join(temp_dir, "%d.pickle.dat" % os.getpid())
//            with open(i_dat_file, "wb") as f:
//                f.write(data["index_data"])
//            self.index.loadIndex(i_file, load_data=True)
//            os.unlink(i_file)
//            os.unlink(i_dat_file)
        }

};
