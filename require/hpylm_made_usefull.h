// hpylm_made_usefull.h

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
#ifndef __HPYLM_MADE_USEFULL_H__
#define __HPYLM_MADE_USEFULL_H__

#include <string>
#include <vector>
#include <fst/fst-decl.h>
#include <fst/matcher.h>

typedef enum {
  kEPS = 0,
  kPHI = 1,
  kSOW = 2,
  kEOW = 3,
  kSOS = 4,
  kEOS = 5
} SpecialWordsType;

class WordMap {
  public:
    WordMap(): _max_index(-1) {};
    void Read(const std::string &filename);


    int operator[](const std::string &word) const;
    int operator[](const std::wstring &word) const;
    std::wstring operator[](const int id) const;

    inline size_t numWords() const {return this->_i2w.size(); };

    void addWord(const std::string &word);
    
  private:
    int _max_index;
    std::unordered_map<std::wstring, int> _w2i;
    std::unordered_map<int, std::wstring> _i2w;

    void _ReadMap(const std::string &filename);

};


typedef std::map<std::string, std::vector<int> > TextWithId;

TextWithId ReadFile(const std::string &filename);
TextWithId ReadFile(const std::string &filename,
                    const WordMap &wordmap, 
                    const int unk);

typedef std::vector<std::vector<int> > TextMapped;

TextMapped ReadTextFile(const std::string &filename, 
                        const WordMap &wordmap, 
                        const int unk,
                        const bool skip_id=true, 
                        const bool fail_on_unk=true);

TextMapped AddBoundaryPadding(const TextMapped &text, 
                              const int order, 
                              const int wbegin, 
                              const int wend);


typedef fst::PhiMatcher<fst::SortedMatcher<fst::Fst<fst::LogArc> > > LogPhiMatcher;

void create_linear_transducer(const std::vector<int> &sentence, 
                              fst::VectorFst<fst::LogArc> *fst);

void append_sentence_end_transition(fst::VectorFst<fst::LogArc> *fst, const int wend);

template<class Arc>
void PhiCompose(const fst::Fst<Arc> &fst1,
                const fst::Fst<Arc> &fst2,
                typename Arc::Label phi_label,
                fst::MutableFst<Arc> *ofst) {
  typedef fst::Fst<Arc> F;
  typedef fst::PhiMatcher<fst::SortedMatcher<F> > PM;
  fst::CacheOptions base_opts;
  base_opts.gc_limit = 0; // Cache only the last state for fastest copy.
  fst::ComposeFstImplOptions<fst::SortedMatcher<F>, PM> impl_opts(base_opts);

  // These pointers are taken ownership of, by ComposeFst.
  PM *phi_matcher =
      new PM(fst2, fst::MATCH_INPUT, phi_label, false);
  fst::SortedMatcher<F> *sorted_matcher = // calling a constructor here ?
      new fst::SortedMatcher<F>(fst1, fst::MATCH_NONE); 
  impl_opts.matcher1 = sorted_matcher;
  impl_opts.matcher2 = phi_matcher;
  *ofst = fst::ComposeFst<Arc>(fst1, fst2, impl_opts);
  Connect(ofst);
}

template<class Arc>
fst::ComposeFst<Arc>  PhiCompose(const fst::Fst<Arc> &fst1,
                                 const fst::Fst<Arc> &fst2,
                                 typename Arc::Label phi_label) {
  typedef fst::Fst<Arc> F;
  typedef fst::PhiMatcher<fst::SortedMatcher<F> > PM;
  fst::CacheOptions base_opts;
  base_opts.gc_limit = 0; // Cache only the last state for fastest copy.
  fst::ComposeFstImplOptions<fst::SortedMatcher<F>, PM> impl_opts(base_opts);

  // These pointers are taken ownership of, by ComposeFst.
  PM *phi_matcher =
      new PM(fst2, fst::MATCH_INPUT, phi_label, false);
  fst::SortedMatcher<F> *sorted_matcher =
      new fst::SortedMatcher<F>(fst1, fst::MATCH_NONE); 
  impl_opts.matcher1 = sorted_matcher;
  impl_opts.matcher2 = phi_matcher;
  return fst::ComposeFst<Arc>(fst1, fst2, impl_opts);
}

void PrintFstPathAsText(std::wostream &out, 
                       const std::vector<int> seq, 
                       const WordMap &wmap);

template<class Arc, class I>
bool GetLinearSymbolSequence(const fst::Fst<Arc> &fst,
                             std::vector<I> *isymbols_out,
                             std::vector<I> *osymbols_out,
                             typename Arc::Weight *tot_weight_out) {
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Weight Weight;

  Weight tot_weight = Weight::One();
  std::vector<I> ilabel_seq;
  std::vector<I> olabel_seq;

  StateId cur_state = fst.Start();
  if (cur_state == fst::kNoStateId) {  // empty sequence.
    if (isymbols_out != NULL) isymbols_out->clear();
    if (osymbols_out != NULL) osymbols_out->clear();
    if (tot_weight_out != NULL) *tot_weight_out = Weight::Zero();
    return true;
  }
  while (1) {
    Weight w = fst.Final(cur_state);
    if (w != Weight::Zero()) {  // is final..
      tot_weight = Times(w, tot_weight);
      if (fst.NumArcs(cur_state) != 0) return false;
      if (isymbols_out != NULL) *isymbols_out = ilabel_seq;
      if (osymbols_out != NULL) *osymbols_out = olabel_seq;
      if (tot_weight_out != NULL) *tot_weight_out = tot_weight;
      return true;
    } else {
      if (fst.NumArcs(cur_state) != 1) return false;

      fst::ArcIterator<fst::Fst<Arc> > iter(fst, cur_state);  // get the only arc.
      const Arc &arc = iter.Value();
      tot_weight = Times(arc.weight, tot_weight);
      if (arc.ilabel != 0) ilabel_seq.push_back(arc.ilabel);
      if (arc.olabel != 0) olabel_seq.push_back(arc.olabel);
      cur_state = arc.nextstate;
    }
  }
}

#endif

