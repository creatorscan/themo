#include "HPYLMFst.hpp"

//const int HPYLMFst::kFileVersion = 1;

void HPYLMFst::InitStateIterator(fst::StateIteratorData< fst::LogArc > *data) const
{
  data->base = 0;
  data->nstates = FinalContextId + 1;
}

void HPYLMFst::InitArcIterator(HPYLMFst::StateId s, fst::ArcIteratorData< fst::LogArc > *data) const
{
  data->base = NULL;
  if (s != FinalContextId) {
    data->arcs = GetArcs(s);
    data->narcs = NumArcs(s);
  } else {
    data->narcs = 0;
    data->arcs = NULL;
  }
  data->ref_count = NULL;
}

const fst::LogArc *HPYLMFst::GetArcs(StateId s) const
{
  if (Arcs.at(s).size() == 0) {
    /* find context for context id */
    const HPYLM::ContextsHashmap::const_iterator it = hpylm.ContextIdToContext.find(s);
    if (it != hpylm.ContextIdToContext.end()) {
      /* extract context sequence and reserve one more element for the following word */
      std::vector<int> WordSequence(it->second->ContextSequence);
      WordSequence.push_back(0);

      /* For the highest order, context for the next state start at the next word*/
      auto NextStateContextIt = WordSequence.begin() + int(WordSequence.size() == hpylm.Order);
      auto CurrentWordIt = WordSequence.end() - 1;

      std::vector<int> Words = (it->second->ContextId > 0) ? it->second->ThisRestaurant.GetWords(ActiveWords) : WordList;
      //Transitions.Probabilities = WordVectorProbability(WHPYLM.GetContextSequence(ContextId - WordContextIdOffset), Transitions.Words);
      for (auto CurrentWord: Words) {
        *CurrentWordIt = CurrentWord;
        assert(BaseProbabilities.find(CurrentWord) != BaseProbabilities.end());        
        Arcs.at(s).push_back(fst::LogArc(CurrentWord, CurrentWord,
                                         -log(hpylm.WordProbability(CurrentWordIt, BaseProbabilities.find(CurrentWord)->second)),
                                         CurrentWord != SentEndWordId ? hpylm.GetContextId(std::vector<int>(NextStateContextIt,CurrentWordIt+1)) 
                                                                      : hpylm.NextUnusedContextId));
      }
      if (it->second->ContextId > 0) {
        *CurrentWordIt = PHI_SYMBOLID;
        Arcs.at(s).push_back(fst::LogArc(PHI_SYMBOLID, EPS_SYMBOLID, 
                                         -log(hpylm.WordProbability(CurrentWordIt, 1.0)),
                                         it->second->PreviousContext->ContextId));
      }
      std::sort(Arcs.at(s).begin(), Arcs.at(s).end(), HPYLMFst::iLabelSort);
    }
  }
  return Arcs.at(s).data();
}
