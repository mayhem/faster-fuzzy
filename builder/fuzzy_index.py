import os
from math import fabs
from time import monotonic
import pickle
import re
import sys

import sklearn
from sklearn.feature_extraction.text import TfidfVectorizer

import nmslib
from unidecode import unidecode
from utils import ngrams

MAX_ENCODED_STRING_LENGTH = 30
NUM_FUZZY_SEARCH_RESULTS = 500

class FuzzyIndex:
    '''
       Create a fuzzy index using a Term Frequency, Inverse Document Frequency (tf-idf)
       algorithm.
    '''

    def __init__(self, name=None):
        self.index_data = None
        self.name = name
        self.index = nmslib.init(method='simple_invindx', space='negdotprod_sparse_fast', data_type=nmslib.DataType.SPARSE_VECTOR)
        self.vectorizer = TfidfVectorizer(min_df=1, analyzer=ngrams)

    @staticmethod
    def encode_string(text):
        """Remove spaces, punctuation, convert non-ascii characters to some romanized equivalent, lower case, return"""
        #TODO: sometimes there are trailing spaces: 'Ji He Xue Mo Yang ' 
        if text is None:
            return None
        return unidecode(re.sub("[ _]+", "", re.sub(r'[^\w ]+', '', text)).strip().lower())[:MAX_ENCODED_STRING_LENGTH]

    @staticmethod
    def encode_string_for_stupid_artists(text):
        """Remove spaces, convert non-ascii characters to some romanized equivalent, lower case, return"""
        if text is None:
            return None
        return unidecode(re.sub("[ _]+", "", text).strip())[:MAX_ENCODED_STRING_LENGTH]

    def build(self, index_data, field):
        if not index_data:
            raise ValueError("No index data passed to index build().")
        self.index_data = index_data
        strings = [x[field] for x in index_data]
        lookup_matrix = self.vectorizer.fit_transform(strings)
        self.index.addDataPointBatch(lookup_matrix, list(range(len(strings))))
        self.index.createIndex()

    def save(self, index_dir):
        v_file = os.path.join(index_dir, "%s_nmslib_vectorizer.pickle" % self.name)
        i_file = os.path.join(index_dir, "%s_nmslib_index.pickle" % self.name)
        d_file = os.path.join(index_dir, "%s_additional_index_data.pickle" % self.name)

        with open(v_file, "wb") as f:
            pickle.dump(self.vectorizer, f)
        self.index.saveIndex(i_file, save_data=True)
        with open(d_file, "wb") as f:
            pickle.dump(self.index_data, f)
            
    def save_to_mem(self, temp_dir):
        # TODO: Use scikit to pickle the vectorizer for better memory performance
        # TODO: Do not save the additional data, its only used for debugging
        vec = pickle.dumps(self.vectorizer)
        additional_data = pickle.dumps(self.index_data)

        i_file = os.path.join(temp_dir, "%d.pickle" % os.getpid())
        self.index.saveIndex(i_file, save_data=True)
        with open(i_file, "rb") as f:
            index = f.read()
        os.unlink(i_file)
        
        i_dat_file = os.path.join(temp_dir, "%d.pickle.dat" % os.getpid())
        with open(i_dat_file, "rb") as f:
            index_data = f.read()
        os.unlink(i_dat_file)

        # Ready for pickling
        return { "vec": vec,
                 "index": index,
                 "index_data": index_data,
                 "additional_data": additional_data }

    def load(self, index_dir):
        v_file = os.path.join(index_dir, "%s_nmslib_vectorizer.pickle" % self.name)
        i_file = os.path.join(index_dir, "%s_nmslib_index.pickle" % self.name)
        d_file = os.path.join(index_dir, "%s_additional_index_data.pickle" % self.name)

        try:
            with open(v_file, "rb") as f:
                self.vectorizer = pickle.load(f)
            self.index.loadIndex(i_file, load_data=True)
            with open(d_file, "rb") as f:
                self.index_data = pickle.load(f)
            return True
        except OSError:
            return False

    def load_from_mem(self, data, temp_dir):

        self.vectorizer = pickle.loads(data["vec"])
        self.index_data = pickle.loads(data["additional_data"])

        i_file = os.path.join(temp_dir, "%d.pickle" % os.getpid())
        with open(i_file, "wb") as f:
            f.write(data["index"])
        i_dat_file = os.path.join(temp_dir, "%d.pickle.dat" % os.getpid())
        with open(i_dat_file, "wb") as f:
            f.write(data["index_data"])
        self.index.loadIndex(i_file, load_data=True)
        os.unlink(i_file)
        os.unlink(i_dat_file)

    def search(self, query_string, min_confidence, debug=False):
        """ Carry out search, returns list of dicts: "text", "id", "confidence" """

        if self.index is None:
            raise IndexError("Must build index before searching")

        query_matrix = self.vectorizer.transform([query_string])
        results = self.index.knnQueryBatch(query_matrix, k=NUM_FUZZY_SEARCH_RESULTS, num_threads=5)
        output = []
        if debug:
            print("Search results for '%s':" % query_string)
        for i, conf in zip(results[0][0], results[0][1]):
            data = self.index_data[i]
            confidence = fabs(conf)
            data["confidence"] = confidence
            if confidence >= min_confidence:
                output.append(data)
                is_below=" "
            else:
                is_below="!"

            if debug:
                print("%c %-30s %10d %.3f" % (is_below, data["text"][:30], data["id"], data["confidence"]))
        
        if debug:
            print()

        return output
