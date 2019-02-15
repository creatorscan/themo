// hpylm_made_usefull.cc

// Copyright (c) 2016, Johns Hopkins University (Yenda Trmal<jtrmal@gmail.com>)

// See ../../COPYING for clarification regarding multiple authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.

#include <iostream>
#include <unordered_map>

#if __GNUC__ >= 5
#include <locale>
#include <codecvt>
#else
#include <locale>
#define BOOST_UTF8_BEGIN_NAMESPACE namespace hpylm_tmp {
#define BOOST_UTF8_END_NAMESPACE }
#include <boost/detail/utf8_codecvt_facet.ipp>
#endif

#include <string>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <fst/fst.h>
#include <fst/fstlib.h>

#include "hpylm_made_usefull.h"

void WordMap::addWord(const std::string &word) {
  std::wstring wword(word.cbegin(), word.cend());
  if (this->_w2i.find(wword) != this->_w2i.end()) {
    //we done here...
  } else {
    this->_max_index++;
    this->_w2i[wword] = this->_max_index;
    this->_i2w[this->_max_index] = wword;
  }
}


int WordMap::operator[](const std::string &word) const {
  return this->operator[](std::wstring(word.cbegin(), word.cend()));
}

int WordMap::operator[](const std::wstring &word) const {
  if (this->_w2i.find(word) != this->_w2i.cend()) {
    //std::wcerr << "Finding a word  " << word  
    //           << " - found " << this->_w2i.at(word) << std::endl;
    return this->_w2i.at(word);
  } else {
    //std::wcerr << "Finding a word  " << word 
    //           <<  " - not found " << std::endl;
    return -1;
  }
}

std::wstring WordMap::operator[](const int id) const {
  if (this->_i2w.find(id) != this->_i2w.end()) {
    return this->_i2w.at(id);
  } else {
    return std::wstring();
  }
}


void WordMap::Read(const std::string &filename) {
  this->_ReadMap(filename);
}

void WordMap::_ReadMap(const std::string &filename) {
  if (filename.empty()) {
    std::cerr << "Error: the filename is empty" << std::endl;
    exit(1);
  } else {
    std::cerr << "Reading " << filename  << std::endl;
  }

  std::wifstream input(filename);

  if (!input) {
    std::cerr << "Error: cannot open the file " << filename << std::endl;
    exit(1);
  }

#if __GNUC__  > 5
  input.imbue(std::locale(std::locale::classic(),
                new std::codecvt_utf8<wchar_t>));
#else
  input.imbue(std::locale(std::locale(), new  hpylm_tmp::utf8_codecvt_facet));
#endif

  int lineno = 0;
  while (input) {
    std::wstring line;

    lineno++;
    std::getline(input, line);
    if (line.empty()) {
      std::cerr << "  Info: empty line " << lineno << std::endl;
      continue;
    }

    std::wstringstream data(line);

    std::wstring word_text;
    int word_id;

    data >> word_text;
    data >> word_id;

    if (!data) {
      std::cerr << "Cannot parse the line " << lineno
        << " in file " << filename << std::endl;
      exit(1);
    }
    if (this->_w2i.find(word_text) != this->_w2i.end()) {
      std::cerr << "Error: word already exists in the wordlist, "
        << " see line " << lineno
        << " in the file " << filename << std::endl;
      exit(1);
    }
    if (this->_i2w.find(word_id) != this->_i2w.end()) {
      std::cerr << "Error: word id already exists in the wordlist, "
        << " see line " << lineno
        << " in the file " << filename << std::endl;
      exit(1);
    }
    this->_w2i[word_text] = word_id;
    this->_i2w[word_id] = word_text;

    if (word_id > this->_max_index) {
      this->_max_index = word_id;
     }
  }
}

void PrintFstPathAsText(std::wostream &out, 
                                 const std::vector<int> seq, 
                                 const WordMap &wmap) {
#if __GNUC__  > 5
  out.imbue(std::locale(std::locale::classic(),
                new std::codecvt_utf8<wchar_t>));
#else
  out.imbue(std::locale(std::locale(), new  hpylm_tmp::utf8_codecvt_facet));
#endif
  if (seq.size() <= 0) {
    out << "[--EMPTY--]";
  } else {
    for (auto &word: seq) {
      out << wmap[word] << " ";
    } 
  }
}

TextWithId ReadFile(const std::string &filename) {

  std::ifstream input(filename);
  TextWithId ret;
  int lineno = 0;
  while (input) {
    std::string line;
    lineno++;
    std::getline(input, line);
  
    if (line.empty()) {
      std::cerr << "  Info: empty line " << lineno << std::endl;
      continue;
    }

    //int strpos = 1;
    std::stringstream data(line);
    std::string id;
    std::vector<int> units;
    data >> id;
    int word;
    while (data >> word) {
      //strpos++;
      units.push_back(word);
    }
    ret[id] = units;
  }
  return ret;
}

TextWithId ReadFile(const std::string &filename,
                    const WordMap &wmap, 
                    const int unk) {
 
  if (unk < 0) {
    std::cerr << "Invalid unk character id supplied (supplied = " 
              << unk << ")" << std::endl;
    exit(1);
  } 
  std::wifstream input(filename);
#if __GNUC__  > 5
  input.imbue(std::locale(std::locale::classic(),
                new std::codecvt_utf8<wchar_t>));
#else
  input.imbue(std::locale(std::locale(), new  hpylm_tmp::utf8_codecvt_facet));
#endif

  TextWithId ret;
  int lineno = 0;
  while (input) {
    std::wstring line;
    lineno++;
    std::getline(input, line);
  
    if (line.empty()) {
      std::cerr << "  Info: empty line " << lineno << std::endl;
      continue;
    }

    int strpos = 1;
    std::wstringstream data(line);
    std::wstring id_tmp;
    data >> id_tmp;
    std::string id(id_tmp.cbegin(), id_tmp.cend());
    
    std::vector<int> units;
    while (data) {
      std::wstring word;
      data >> word;

      strpos++;

      if (wmap[word] == -1) {
        units.push_back(unk);
      } else {
        units.push_back(wmap[word]);
      }
    }
    ret[id] = units;
  }
  return ret;
}

TextMapped ReadTextFile(const std::string &filename, 
                       const WordMap &wordmap, 
                       const int unk,
                       const bool skip_id, 
                       const bool fail_on_unk) {

  if (filename.empty()) {
    std::cerr << "Error: the filename is empty" << std::endl;
    exit(1);
  }

  std::wifstream input(filename);

  if (unk == -1) {
    std::cerr << "Error: unk word <unk> not found in the word list" << std::endl;
    exit(1);
  }

  if (!input) {
    std::cerr << "Error: cannot open the file " << filename << std::endl;
    exit(1);
  }

#if __GNUC__  > 5
  input.imbue(std::locale(std::locale::classic(),
                new std::codecvt_utf8<wchar_t>));
#else
  input.imbue(std::locale(std::locale(), new  hpylm_tmp::utf8_codecvt_facet));
#endif

  int lineno = 0;
  TextMapped text_mapped;

  while (input) {
    std::wstring line;

    lineno++;
    std::getline(input, line);
    if (line.empty()) {
      std::cerr << "  Info: empty line " << lineno << std::endl;
      continue;
    }

    std::wstringstream data(line);
    std::vector<int> words_mapped;

    int strpos = 0;
    while (data) {
      std::wstring word;
      data >> word;

      // just in case there were extra white characters
      // at the end of the line
      if (word.empty())
        continue;

      strpos++;
      if (skip_id && (strpos == 1))
        std::cerr << strpos;
        continue;

      int idx = wordmap[word];
      if (idx != -1) {
        words_mapped.push_back(idx);
      } else {
        if (fail_on_unk) {
          std::cerr << "Error: cannot map a word in file " << filename
            << " on the line " << lineno
            << " and the position " << strpos << std::endl;
          exit(1);
        }
        words_mapped.push_back(unk);
      }
    }
    text_mapped.push_back(words_mapped);
  }
  return text_mapped;
}

TextMapped AddBoundaryPadding(const TextMapped &text, const int order, const int wbegin, const int wend) {
  TextMapped output;

  for(auto &sentence: text) {
    std::vector<int> padded_sentence;
    padded_sentence.reserve(order + sentence.size());
    for(int i = 0; i < order; i++) {
      padded_sentence.push_back(wbegin);
    }
    std::copy(sentence.cbegin(), sentence.cend(),
        std::back_insert_iterator< std::vector<int> >(padded_sentence));
    padded_sentence.push_back(wend);
    output.push_back(padded_sentence);
  }
  return output;
}


void create_linear_transducer(const std::vector<int> &sentence, 
                              fst::VectorFst<fst::LogArc> *fst) {

  fst::LogArc::StateId i = fst->AddState(); 
  fst->SetStart(i);
  for(auto &phone: sentence) {
    fst::LogArc::StateId i_new =  fst->AddState();
    fst->AddArc(i, fst::LogArc(phone, phone, 0, i_new));
    i = i_new;
  }
  fst->SetFinal(i, 0); 

}

