#pragma once

#include <stdio.h>
#include <iostream>
#include <map>
#include <string>
#include <vector>
using namespace std;

#include "defs.hpp"

#include "jpcre2.hpp"
#include "unidecode/unidecode.hpp"
#include "unidecode/utf8_string_iterator.hpp"
#include "tfidf_vectorizer.hpp"
#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>

typedef jpcre2::select<char> jp; 


class EncodeSearchData {
    private:
        jp::Regex                *non_word;
        jp::Regex                *spaces_uscore;
        jp::Regex                *spaces;

    public:

        EncodeSearchData() {
            non_word = new jp::Regex();
            spaces_uscore = new jp::Regex();
            spaces = new jp::Regex();

            non_word->setPattern("[^\\w]+").addModifier("n").compile();
            spaces_uscore->setPattern("[ _]+").addModifier("n").compile();
            spaces->setPattern("[\\s]+").addModifier("n").compile();
        }
        
        ~EncodeSearchData() {
            delete non_word;
            delete spaces;
            delete spaces_uscore;
        }

        string unidecode(const string &str) {
            unidecode::Utf8StringIterator begin = str.c_str();
            unidecode::Utf8StringIterator end = str.c_str() + str.length();
            string output;
            unidecode::Unidecode(begin, end, std::back_inserter(output));
            return output;
        }
        
        string
        encode_string(const string &text) {
            // Remove spaces, punctuation, convert non-ascii characters to some romanized equivalent, lower case, return
            if (text.empty()) {
                string a;
                return a;
            }
            string cleaned(non_word->replace(text,"", "g"));
            cleaned = unidecode(cleaned);
            transform(cleaned.begin(), cleaned.end(), cleaned.begin(), ::tolower);
            // Sometimes unidecode puts spaces in, so remove them
            string ret(spaces_uscore->replace(cleaned,"", "g"));

            return ret;
        }

        string
        encode_string_for_stupid_artists(const string &text) {
            //Remove spaces, convert non-ascii characters to some romanized equivalent, lower case, return
            if (text.empty()) {
                string a;
                return a;
            }

            string cleaned(spaces->replace(text,"", "g"));
            transform(cleaned.begin(), cleaned.end(), cleaned.begin(), ::tolower);
            return cleaned;
        }

        void 
        encode_index_data(const vector<unsigned int> &input_ids, const vector<string> &input_texts,
                          vector<unsigned int> &output_ids,
                          vector<string>       &output_texts,
                          vector<unsigned int> &stupid_ids,
                          vector<string>       &stupid_texts) {
            for(unsigned int i = 0; i < input_ids.size(); i++) {
                auto ret = encode_string(input_texts[i]);
                if (ret.size() == 0) {
                    auto stupid = encode_string_for_stupid_artists(input_texts[i]);
                    if (stupid.size()) {
                        stupid_ids.push_back(input_ids[i]);
                        stupid_texts.push_back(stupid);
                        continue;
                    }
                }
                output_ids.push_back(input_ids[i]);
                output_texts.push_back(ret);
            }
        }
};