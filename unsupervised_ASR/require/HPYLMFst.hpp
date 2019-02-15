#ifndef _HPYLMFST_HPP_
#define _HPYLMFST_HPP_

#include <fst/fst.h>
#include "HPYLM.hpp"
#include "NHPYLM/definitions.hpp"

using std::vector;
using std::string;
using std::cout;
using std::endl;
using std::cerr;

//#define EPS_SYMBOL         "<eps>"
#define EPS_SYMBOLID       0
//#define PHI_SYMBOL         "<phi>"
#define PHI_SYMBOLID       1

/* fst for nested hierachical pitman yor language model */
class HPYLMFst : public fst::Fst<fst::LogArc> {
  typedef fst::LogArc::StateId StateId; // state ids
  typedef fst::LogArc::Weight Weight;   // weights

  //static const int kFileVersion;
  const HPYLM &hpylm;    // reference to nested hierarchical pitman yor language model
  std::vector<int> WordList;
  std::vector<int> StartContext;
  const int SentEndWordId;        // sentence and word id
  const int StartContextId;       // id of start context
  const int FinalContextId;       // id of final context
  const uint64 FSTProperties;     // properties of fst
  const string FSTType;      // type of fst
  const google::dense_hash_map<int, double> &BaseProbabilities;
  const vector<bool> &ActiveWords; // vector indicating active words
  mutable std::vector<std::vector<fst::LogArc> > Arcs; // vector containing arcs of all states

  /* internal functions */
  // get transitions from language model and build arcs
  const fst::LogArc *GetArcs(StateId s) const;

  // used to sort the arcs according to the imput label
  static inline bool iLabelSort(const fst::LogArc &i, const fst::LogArc &j) { return i.ilabel < j.ilabel; }
public:
  /* constructor and destructor */
  // setup the fst for the nested hierarchical pitman yor language model
  HPYLMFst(
    const HPYLM &hpylm_,
    std::vector<int> WordList_,
    std::vector<int> StartContext_,
    int SentEndWordId_,
    const google::dense_hash_map<int, double> &BaseProbabilities_,
    const vector<bool> &ActiveWords_
  ) :
    hpylm(hpylm_),
    WordList(WordList_),
    StartContext(StartContext_),
    SentEndWordId(SentEndWordId_),
    StartContextId(hpylm_.GetContextId(StartContext_)),
    FinalContextId(hpylm_.GetNextUnusedContextId()),
    FSTProperties(fst::kOEpsilons | fst::kILabelSorted | fst::kOLabelSorted),
    FSTType("vector"),
    BaseProbabilities(BaseProbabilities_),
    ActiveWords(ActiveWords_),
    Arcs(hpylm_.GetNextUnusedContextId() + 1) {}

  /* interface */
  // Initial state
  StateId Start() const { return StartContextId; }

  // State's final weight
  Weight Final(StateId s) const { return s == FinalContextId ? Weight::One() : Weight::Zero(); }

  // State's arc count
  size_t NumArcs(StateId s) const { return  Arcs.at(s).size(); }

  // State's input epsilon count
  size_t NumInputEpsilons(HPYLMFst::StateId s) const { return 0; }

  // State's output epsilon count
  size_t NumOutputEpsilons(StateId s) const { return ((s != 0) && (s != FinalContextId)); }

  // Property bits
  uint64 Properties(uint64 mask, bool) const { return mask & FSTProperties; }

  // Fst type name
  const string &Type() const { return FSTType; }

  // Get a copy of this Fst
  Fst<fst::LogArc> *Copy(bool = false) const { return new HPYLMFst(hpylm, WordList, StartContext, SentEndWordId, BaseProbabilities, ActiveWords); } 

  // Return input label symbol table; return NULL if not specified
  const fst::SymbolTable *InputSymbols() const { return NULL; }

  // Return output label symbol table; return NULL if not specified
  const fst::SymbolTable *OutputSymbols() const { return NULL; }

  // For generic state iterator construction
  void InitStateIterator(fst::StateIteratorData<fst::LogArc> *data) const;
  
  // For generic arc iterator construction
  void InitArcIterator(StateId s, fst::ArcIteratorData<fst::LogArc> *data) const;
};

#endif
