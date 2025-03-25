import os

import sklearn
from sklearn.feature_extraction.text import TfidfVectorizer

import nmslib

def ngrams(string, n=3):
    """ Take a lookup string (noise removed, lower case, etc) and turn into a list of trigrams """
    ngrams = zip(*[string[i:] for i in range(n)])
    return [''.join(ngram) for ngram in ngrams]

index = nmslib.init(method='simple_invindx', space='negdotprod_sparse_fast', data_type=nmslib.DataType.SPARSE_VECTOR)
vectorizer = TfidfVectorizer(min_df=1, analyzer=ngrams)

strings = ["This is the first document.", 
           "This document is the second document.", 
           "And this is the third one.", 
           "Is this the first document?"];

lookup_matrix = vectorizer.fit_transform(strings)
print(lookup_matrix)
index.addDataPointBatch(lookup_matrix, list(range(len(strings))))
index.createIndex()
