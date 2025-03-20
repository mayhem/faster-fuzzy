// From https://github.com/taozhaojie/TFIDF_cpp/blob/master/tfidf_vector.cpp
// TODO: License is unclear
#include <iostream>
#include <fstream>
#include <set>
#include <boost/tokenizer.hpp>
#include <sstream>

using namespace std;

class TfidfVectorizer {
private:
	std::vector<std::vector<float>> dataMat; // converted bag of words matrix
	unsigned int nrow; // matrix row number
	unsigned int ncol; // matrix column number
	std::vector<std::string> rawDataSet; // raw data
	std::vector<std::string> vocabList; // all terms
	std::vector<int> numOfTerms; // used in tf calculation
	
	void createVocabList()
	{
		std::set<std::string> vocabListSet;
		for (std::string word : rawDataSet)
			vocabListSet.insert(word);
		std::copy(vocabListSet.begin(), vocabListSet.end(), std::back_inserter(vocabList));
	}

	inline std::vector<float> word2VecMN(const std::string &word)
	{
		std::vector<float> returnVec(vocabList.size(), 0);
		size_t idx = std::find(vocabList.begin(), vocabList.end(), word) - vocabList.begin();
		if (idx == vocabList.size())
			cout << "word: " << word << "not found" << endl;
		else
			returnVec.at(idx) += 1;
		return returnVec;
	}

	void vec2mat()
	{
		int cnt(0);
		for (auto it : rawDataSet)
		{
			cnt ++;
			cout << cnt << "\r";
			std::cout.flush();
			dataMat.push_back(word2VecMN(it));
			numOfTerms.push_back(it.size());
			it.clear();
		}
		cout << endl;
		ncol = dataMat[0].size();
		nrow = dataMat.size();
		rawDataSet.clear(); // release memory
	}

	inline std::vector<float> vec_sum(const std::vector<float>& a, const std::vector<float>& b)
	{
		assert(a.size() == b.size());
		std::vector<float> result;
		result.reserve(a.size());
		std::transform(a.begin(), a.end(), b.begin(), 
					   std::back_inserter(result), std::plus<float>());
		return result;
	}

	void calMat()
	{
		createVocabList();
		vec2mat();

		std::vector<std::vector<float>> dataMat2(dataMat);
		std::vector<float> termCount;
		termCount.resize(ncol);

		for (unsigned int i = 0; i != nrow; ++i)
		{
			for (unsigned int j = 0; j != ncol; ++j)
			{
				if (dataMat2[i][j] > 1) // only keep 1 and 0
					dataMat2[i][j] = 1;
			}
			termCount = vec_sum(termCount, dataMat2[i]); // no. of doc. each term appears
		}
		dataMat2.clear(); //release

		std::vector<float> row_vec;
		for (unsigned int i = 0; i != nrow; ++i)
		{
			for (unsigned int j = 0; j != ncol; ++j)
			{
				float tf = dataMat[i][j] / numOfTerms[i];
				float idf = log((float)nrow / (termCount[j]));
				row_vec.push_back(tf * idf); // TF-IDF equation
			}
			weightMat.push_back(row_vec);
			row_vec.clear();
		}
		nrow = weightMat.size();
		ncol = weightMat[0].size();
	}

public:
	std::vector<std::vector<float>> weightMat; // TF-IDF weighting matrix
	TfidfVectorizer(std::vector<std::string> & input):rawDataSet(input)
	{
		calMat();
	}
};