#pragma once
#include <stdio.h>
#include "fuzzy_index.hpp"


using namespace std;

//"artist_data.txt
//"stupid_artist_data.txt
//
class ArtistIndexes {
    private:
        FuzzyIndex      index;
        string          index_dir; 

    public:

        ArtistIndexes(const string &_index_dir) { 
            index_dir = _index_dir;
        }
        
        ~ArtistIndexes() {
        }
        
        // 幾何学模様 a                  Kikagaku Moyo c
        void build_artist_index(string &artist_data_file, bool remove_data_file) {
            
            vector<unsigned int> index_ids;
            vector<string>       index_texts;
            string               file = index_dir;

            file += string("/") + artist_data_file;
            auto *fp = fopen(file.c_str(), "rb");
            if (fp == nullptr)
                throw std::invalid_argument("File not found.\n");
                    
            for(;;) {
                unsigned int id, length;

                if (fread(&id, 1, sizeof(id), fp) != sizeof(id))
                    break;
                if (fread(&length, 1, sizeof(length), fp) != sizeof(length))
                    throw std::invalid_argument("unexpected EOF.\n");
                
                char *str = (char *)malloc(length+1);
                if (fread(str, 1, length, fp) != length)
                    throw std::invalid_argument("unexpected EOF.\n");
                
                *(str + length) = 0;
                index_texts.push_back(string(str));
                index_ids.push_back(id);
                free(str);
            }
            fclose(fp);
            if (remove_data_file)
                remove(artist_data_file.c_str());
           
            index.build(index_ids, index_texts);
        }
};
