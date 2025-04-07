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
        unsigned int              max_string_length;

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
            if (ret.length() > max_string_length)
                remainder = ret.substr(max_string_length);
            
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
        
            auto main_part = cleaned.substr(0, max_string_length);
            string remainder;
            if (cleaned.length() > max_string_length)
                remainder = cleaned.substr(max_string_length);
            
            vector<string> out = { main_part, remainder};
            return out;
        }

        void 
        encode_index_data(const vector<unsigned int> &input_ids, const vector<string> &input_texts,
                          vector<unsigned int> &output_ids,
                          vector<string>       &output_texts,
                          vector<string>       &output_rems,
                          vector<unsigned int> &stupid_ids,
                          vector<string>       &stupid_texts,
                          vector<string>       &stupid_rems) {
            for(unsigned int i = 0; i < input_ids.size(); i++) {
                auto ret = encode_string(input_texts[i]);
                if (ret[0].size() == 0) {
                    printf("stupid '%s'\n", input_texts[i].c_str());
                    auto stupid = encode_string_for_stupid_artists(input_texts[i]);
                    if (stupid[0].size()) {
                        stupid_ids.push_back(input_ids[i]);
                        stupid_texts.push_back(stupid[0]);
                        stupid_rems.push_back(stupid[1]);
                        continue;
                    }
                }
                output_ids.push_back(input_ids[i]);
                output_texts.push_back(ret[0]);
                output_rems.push_back(ret[1]);
                printf("'%s'\n", ret[0].c_str());
            }
        }
};