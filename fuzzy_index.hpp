#include <iostream>
#include <map>
#include <string>
#include <regex>
#include <vector>
#include <bits/stdc++.h>
using namespace std;

#include "unidecode/unidecode.hpp"
#include "unidecode/utf8_string_iterator.hpp"


const auto MAX_ENCODED_STRING_LENGTH = 30;
const auto NUM_FUZZY_SEARCH_RESULTS = 500;

class IndexData {
    int id;
    string text;
};

class FuzzyIndex {
    private:
        shared_ptr<vector<IndexData>> index_data; 

    public:

        FuzzyIndex(const string &_name) {
            index_data = shared_ptr<vector<IndexData>>(new vector<IndexData>());
            string name = _name;
//            index = nmslib.init(method='simple_invindx', space='negdotprod_sparse_fast', data_type=nmslib.DataType.SPARSE_VECTOR)
//            vectorizer = TfidfVectorizer(min_df=1, analyzer=ngrams)
        }
        
        ~FuzzyIndex() {
        }

        static string unidecode(const string &str) {
            unidecode::Utf8StringIterator begin = str.c_str();
            unidecode::Utf8StringIterator end = str.c_str() + str.length();
            string output;
            unidecode::Unidecode(begin, end, std::back_inserter(output));
            return output;
        }

        static string encode_string(string &text) {
            // Remove spaces, punctuation, convert non-ascii characters to some romanized equivalent, lower case, return
            if (text.empty())
                return text; 
           
            cout << text << endl;
            text = regex_replace(text, regex("[_ ]|[^\\w\\u0080-\\uffff]+"), "");
            cout << text << endl;
            cout << text << endl;
            transform(text.begin(), text.end(), text.begin(), ::tolower);
            // Sometimes unidecode puts spaces in, so remove them
            text = unidecode(text);
            text = regex_replace(text, regex("[ ]+"), "");
            return text;
        }

        static string encode_string_for_stupid_artists(string &text) {
            //Remove spaces, convert non-ascii characters to some romanized equivalent, lower case, return
            if (text.empty())
                return text; 
            text = regex_replace(text, regex("[\\s]+"), "");
            // TODO: split according to len
            return unidecode(text);
        }

        void build(vector<IndexData> &index_data) {
            if (index_data.size() == 0)
                throw std::length_error("no index data provided.");

//            self.index_data = index_data
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
