#include <stdio.h>

#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <bits/stdc++.h>
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

class FuzzyIndex {
    private:
        shared_ptr<vector<IndexData>> index_data; 
        
        // Couldn't get smart_ptr to compile here. odd.
        jp::Regex *non_word;
        jp::Regex *spaces_uscore;
        jp::Regex *spaces;

    public:

        FuzzyIndex(const string &_name) {
            index_data = shared_ptr<vector<IndexData>>(new vector<IndexData>());
            string name = _name;
            non_word = new jp::Regex();
            spaces_uscore = new jp::Regex();
            spaces = new jp::Regex();

            non_word->setPattern("[^\\w]+").addModifier("n").compile();
            spaces_uscore->setPattern("[ _]+").addModifier("n").compile();
            spaces->setPattern("[\\s]+").addModifier("n").compile();
            
            similarity::initLibrary(0, LIB_LOGNONE, NULL);
//            index = nmslib.init(method='simple_invindx', space='negdotprod_sparse_fast', data_type=nmslib.DataType.SPARSE_VECTOR)
//            vectorizer = TfidfVectorizer(min_df=1, analyzer=ngrams)
        }
        
        ~FuzzyIndex() {
            delete non_word;
            delete spaces;
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

        vector<string> encode_string_for_stupid_artists(const string &text) {
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

        void build(vector<IndexData> &index_data) {
            if (index_data.size() == 0)
                throw std::length_error("no index data provided.");

//            this->index_data = index_data;
            similarity::AnyParams index_params({ "NN=11", "efConstruction=50", "indexThreadQty=4" });
            similarity::AnyParams query_time_params( { "efSearch=50" });
            //(method='simple_invindx', space='', data_type=nmslib.DataType.SPARSE_VECTOR
            // Object(IdType id, LabelType label, size_t datalength, const void* data)
            similarity::ObjectVector data;
            vector<string> text_data;
            for(auto entry : index_data) {
                // TODO: Review this, compare to binding
                data.push_back(new similarity::Object(entry.id, 45, entry.text.length(), entry.text.c_str()));
                text_data.push_back(entry.text);
            }
            
        	TfidfVectorizer vectorizer(text_data);
        	vector<std::vector<float>> space_data = vectorizer.weightMat;
           
            // method='simple_invindx', space='negdotprod_sparse_fast', data_type=nmslib.DataType.SPARSE_VECTOR)
            // auto index = new IndexWrapper<float>(method, space, space_params, data_type, dtype);
            auto space = similarity::SpaceFactoryRegistry<float>::Instance().CreateSpace("negdotprod_sparse_fast",
                                                                                         similarity::AnyParams());
            similarity::Index<float> *index =  
                            similarity::MethodFactoryRegistry<float>::Instance().CreateMethod(true,
                                "simple_invindx",
                                "negdotprod_sparse_fast",
                                 *space,
                                 data);
            
            // TODO: delete space?

//            index->CreateIndex(index_params);
//            strings = [x["text"] for x in index_data]
//            lookup_matrix = self.vectorizer.fit_transform(strings)
//            self.index.addDataPointBatch(lookup_matrix, list(range(len(strings))))
//            self.index.createIndex()
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

        void search(string &query_string, float min_confidence, bool debug=false) {
            // Carry out search, returns list of dicts: "text", "id", "confidence" 
//
//            if self.index is None:
//                raise IndexError("Must build index before searching")
//
//            query_matrix = self.vectorizer.transform([query_string])
//#        t0 = monotonic()
//            results = self.index.knnQueryBatch(query_matrix, k=NUM_FUZZY_SEARCH_RESULTS, num_threads=5)
//#        print("search time: %.2fms" % ((monotonic() - t0) * 1000))
//            output = []
//            if debug:
//                print("Search results for '%s':" % query_string)
//            for i, conf in zip(results[0][0], results[0][1]):
//                data = self.index_data[i]
//                confidence = fabs(conf)
//                data["confidence"] = confidence
//                if confidence >= min_confidence:
//                    output.append(data)
//                    is_below=" "
//                else:
//                    is_below="!"
//
//                if debug:
//                    print("%c %-30s %10d %.3f" % (is_below, data["text"][:30], data["id"], data["confidence"]))
//            
//            if debug:
//                print()
//
//            return output
        }
};
