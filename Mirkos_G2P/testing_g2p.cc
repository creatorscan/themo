#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
//#include <unordered_map>
#include <unordered_set>
#include <stdlib.h>
//#include <time.h>
#include <fst/fst.h>
#include <fst/fst-decl.h>
#include <fst/fstlib.h>
#include <fst/extensions/far/far.h>
#include <fst/vector-fst.h>
//#include "HPYLMFst.hpp"

//#include <boost/program_options.hpp>
//namespace po = boost::program_options;


using namespace fst;
typedef LogArc MyArc;
/* Yenda has these
const int kPHI=1;
const int kSOS=2;
const int kEOS=2;
*/
typedef enum {
  kEPS = 0,
  kPHI = 1,
  //  kSOW = 2,
  //  kEOW = 3,
  kSOS = 2,
  kEOS = 2
} SpecialWordsType;


template <class Arc>
ComposeFst<Arc> RPhiCompose(const Fst<Arc>& fst1, const Fst<Arc>& fst2, typename Arc::Label phi_label) {
  typedef PhiMatcher<Matcher<Fst<Arc> > > PM;
  ComposeFstOptions<Arc, PM> copts(CacheOptions(), new PM(fst1, MATCH_NONE, kNoLabel), new PM(fst2, MATCH_INPUT, phi_label));
  return ComposeFst<Arc>(fst1, fst2, copts);
};


template<class Arc>
void PhiCompose(const Fst<Arc> &fst1, const Fst<Arc> &fst2, typename Arc::Label phi_label, MutableFst<Arc> *ofst) {
  typedef SortedMatcher<Fst<Arc> > SM;
  typedef PhiMatcher<SM>                PM;
  CacheOptions base_opts;
  base_opts.gc_limit = 0; // Cache only the last state for fastest copy.
  // ComposeFstImplOptions templated on matcher for fst1, matcher for fst2.
  // The matcher for fst1 doesn't matter; we'll use fst2's matcher.
  ComposeFstImplOptions<SM, PM> impl_opts(base_opts);
  // These pointers are taken ownership by ComposeFst.
  impl_opts.matcher1 = new SM(fst1, MATCH_NONE);
  impl_opts.matcher2 = new PM(fst2, MATCH_INPUT, phi_label, false);
  *ofst = ComposeFst<Arc>(fst1, fst2, impl_opts);
  Connect(ofst);
}

template<class Arc>
void RemoveWeights(MutableFst<Arc> *ofst) { // Replaces all non-zero weights with Weight::One().
  typedef typename Arc::Weight Weight;
  for(StateIterator<Fst<Arc>> siter(*ofst); !siter.Done(); siter.Next()) {
    typename Arc::StateId s = siter.Value();
    // Remove final weight.
    Weight w_final = ofst->Final(s);
    if (w_final != Weight::Zero()) { // Is a final state.
      Weight new_w = Weight::One();
      ofst->SetFinal(s, new_w);
    }
    // Go through all states and remove weights from arcs.
    for(MutableArcIterator<MutableFst<Arc>> aiter(ofst, s); !aiter.Done(); aiter.Next()) {
      Arc arc = aiter.Value();
      Weight w = arc.weight;
      Weight new_w = Weight::One();
      arc.weight = new_w;
      aiter.SetValue(arc);
    }
  }
}

int main(int argc, char *argv[]) {
  //srand (time(NULL));
  srand(1234);

  const char *far_phone_name = argv[1];//"../data_dict/test_prons.far";
  const char *far_graph_name = argv[2];//"../data_dict/test_words.far";
  const char *fst_p2g_name = argv[3];//"../data_dict/train_p2g.fst";
  const char *fst_g2l_name = argv[4];//"../data_dict/train_g2l.fst";
  const char *fst_e_lim_name = argv[5];//"../data_dict/train_limiter.fst";
  const char *fst_ledit_name = argv[6];//"../data_dict/letter_edit.fst";
  const char *fst_pedit_name = argv[7];//"../data_dict/phone_edit.fst";

  const char *fst_g_hpylm_name = argv[8];//"../dict_sevengram/hpylm.final.fst";

  vector<std::pair<const Fst<MyArc> *,const Fst<MyArc> *>> lexicon;

  FarReader<MyArc> *far_phone_reader = FarReader<MyArc>::Open(far_phone_name);
  if (!far_phone_reader) {
    std::cerr << "Cannot open phonemic FAR: " << far_phone_name << std::endl;
    return -1;
  }
  FarReader<MyArc> *far_graph_reader = FarReader<MyArc>::Open(far_graph_name);
  if (!far_graph_reader) {
    std::cerr << "Cannot open graphemic FAR: " << far_graph_name << std::endl;
    return -1;
  }
  unsigned num_examples = 0;
  for(; !far_phone_reader->Done() && !far_graph_reader->Done(); far_phone_reader->Next(), far_graph_reader->Next()) {
    //string key = far_reader->GetKey();
    lexicon.push_back(std::make_pair(far_graph_reader->GetFst()->Copy(), far_phone_reader->GetFst()->Copy()));
    num_examples++;
  }
  if(!far_phone_reader->Done() || !far_graph_reader->Done()) {
    std::cerr << "The graphemic and phonemic FARs do not have the same number of records: " << far_graph_name << ", " << far_phone_name << std::endl;
    return -1;
  }
  Fst<MyArc> *fst_p2g = Fst<MyArc>::Read(fst_p2g_name);
  Fst<MyArc> *fst_g2l = Fst<MyArc>::Read(fst_g2l_name);
  Fst<MyArc> *fst_lim = Fst<MyArc>::Read(fst_e_lim_name);
  if(fst_p2g == NULL || fst_g2l == NULL) {
    std::cerr << "Cannot open phone2graphone or graphone2grepheme transducer: " << fst_p2g_name << ", " << fst_g2l_name << std::endl;
    return -1;
  }
  if(fst_p2g->Final(fst_p2g->Start()) == MyArc::Weight::Zero() || fst_g2l->Final(fst_g2l->Start()) == MyArc::Weight::Zero()) {
    std::cerr << "Invalid multistate phone2graphone or graphone2grepheme transducer: " << fst_p2g_name << ", " << fst_g2l_name << std::endl;
    return -1;
  }

  Fst<MyArc> *fst_ledit = Fst<MyArc>::Read(fst_ledit_name);
  Fst<MyArc> *fst_pedit = Fst<MyArc>::Read(fst_pedit_name);
  if(fst_ledit == NULL || fst_pedit == NULL) {
    std::cerr << "Cannot open letter edit and phone edit transducer: " << fst_ledit_name << ", " << fst_pedit_name << std::endl;
    return -1;
  }
  if(fst_ledit->Final(fst_ledit->Start()) == MyArc::Weight::Zero() || fst_pedit->Final(fst_pedit->Start()) == MyArc::Weight::Zero()) {
    std::cerr << "Invalid multistate letter and phone edit transducer: " << fst_ledit_name << ", " << fst_pedit_name << std::endl;
    return -1;
  }

  Fst<MyArc> *fst_g_hpylm = Fst<MyArc>::Read(fst_g_hpylm_name);
  if(fst_g_hpylm == NULL) {
    std::cerr << "Cannot open HPYLM transducer: " << fst_g_hpylm_name << std::endl;
    return -1;
  }

  std::cout << "test examples: " << num_examples << std::endl;

  //##########################################
  //############ Main loop ###################
  //##########################################

  bool limit_epsilon = false;
  double testLL = 0.0, lastLL = 0.0;
  int del = 0, sub = 0, ins = 0, perr = 0, pref = 0;
  {
    for(int ind = 0; ind < num_examples; ind++) {
      // Get phone and letter strings
      auto lex_entry = lexicon[ind];

      VectorFst<MyArc> fst_g2out;
      Compose(*fst_g2l ,*lex_entry.first, &fst_g2out);
      Project(&fst_g2out, PROJECT_INPUT);

      VectorFst<MyArc> fst_p2g_out;
      if (limit_epsilon) {
        VectorFst<MyArc> fst_g2out_lim;
        fst::Compose(fst_g2out, *fst_lim, &fst_g2out_lim);
        Compose(*fst_p2g, fst_g2out_lim, &fst_p2g_out);
      } else {
        Compose(*fst_p2g, fst_g2out, &fst_p2g_out);
      }

      fst_p2g_out.Write("fst_p2g2outlg_lim.fst");
      VectorFst<MyArc> fst_p2G;
      PhiCompose(fst_p2g_out, *fst_g_hpylm, kPHI, &fst_p2G);
      fst_p2G.Write("fst_p2G.fst");
      if (fst_p2G.NumStates() == 0) {
        std::cerr << "WARNING: Empty composition in line:" << ind << std::endl;
        continue;
        exit(0);
      }
      // Sum all graphone paths with the same letter sequence.
      Project(&fst_p2G, PROJECT_INPUT);
      //VectorFst<MyArc> fst_p2G_det;
      //Determinize(fst_p2G, &fst_p2G_det); // not possible, there are loops
      //LogVectorFst pruned_fst;
      //prune_fst(fst_p2G, &pruned_fst, 2);

      // Decode the best path.
      StdVectorFst fst_decode;
      Cast(fst_p2G, &fst_decode);
      StdVectorFst best_path;

      std::vector<StdArc::Weight> distance;
      //typedef AnyArcFilter<StdArc> ArcFilter;
      //typedef AutoQueue<StdArc::StateId> Queue;
      //Queue *queue = new AutoQueue<StdArc::StateId>(fst_decode, &distance, ArcFilter());
      //ShortestPathOptions<StdArc, Queue, ArcFilter> spopts(queue, ArcFilter(), 5, false); // number of paths, true: with unique input labels
      //ShortestPath(fst_decode, &best_path, &distance, spopts);
      //delete queue;
      ShortestPath(fst_decode, &best_path);
      assert(best_path.NumStates() > 0);
      //  std::cerr << "WARNING: Empty best path:" << ind << std::endl;
      //  continue;
      RmEpsilon(&best_path);
      ShortestDistance(best_path, &distance);
      double bestLL = distance[0].Value();
      testLL += bestLL;

      // Print best path.
      StdVectorFst::StateId sid = best_path.Start();
      StdVectorFst::StateId dest;
      while(best_path.Final(sid) == StdArc::Weight::Zero()) {
        ArcIterator<StdVectorFst> aiter(best_path, sid);
        dest = aiter.Value().nextstate;
        assert(dest != kNoStateId);
        sid = dest;
        int ilabel = aiter.Value().ilabel;
        int olabel = aiter.Value().olabel;
        //std::cout << " " << ilabel << ":" << olabel;
        //std::cout << " " << ilabel;
        std::cout << " " << olabel;
      }//*/


      // Compute edit distance.
      VectorFst<MyArc> reference_edit;
      Compose(*fst_pedit, *lex_entry.second, &reference_edit);
      StdVectorFst ref_edit;
      Cast(reference_edit, &ref_edit);
      StdVectorFst edit_dist;
      RemoveWeights(&best_path);
      Compose(best_path, ref_edit, &edit_dist);
      assert(edit_dist.NumStates() > 0);
      StdVectorFst best_edit;
      ShortestPath(edit_dist, &best_edit);
      assert(best_edit.NumStates() > 0);

      // Count insertions, substitutions and deletions.
      StdVectorFst::StateId state_id = best_edit.Start();
      StdVectorFst::StateId dest_state;
      int i = 0, isub = 0, idel = 0, iins = 0;
      while(best_edit.Final(state_id) == StdArc::Weight::Zero()) {
        ArcIterator<StdVectorFst> aiter(best_edit, state_id);
        dest_state = aiter.Value().nextstate;
        assert(dest_state != kNoStateId);
        state_id = dest_state;
        int ilabel = aiter.Value().ilabel;
        int olabel = aiter.Value().olabel;
        if (olabel != 0) pref++;
        if (ilabel == olabel) {
          std::cout << " " << ilabel;
        } else {
          perr++;
          if (ilabel == 0) idel++;
          if (olabel == 0) iins++;
          if (ilabel != 0 && olabel != 0) isub++;
          //std::cout << " " << ilabel << ":" << olabel;
          //std::cout << " " << olabel;
        }
      }
      pref--; // Do not count sentence end.
      sub += isub;
      del += idel;
      ins += iins;
      std::cout << " ( " << bestLL << " " << isub << " sub " << idel << " del " << iins << " ins )" << std::endl;

      if (ind == -1) {
        lex_entry.second->Write("fst_in.fst");
        lex_entry.first->Write("fst_out.fst");
        fst_p2G.Write("fst_decode.fst");
        best_path.Write("fst_bestpath.fst");
        best_edit.Write("fst_bestedit.fst");
        exit(0);
      }

      if (ind % 200 == 199) {
        std::cout << "decoding " << ind + 1 << " " << testLL - lastLL << std::endl;
        double PER = perr;
        PER = PER * 100.0 / pref;
        std::cout << "PER " << PER << "% phone errors: " << perr << " reference phones: " << pref << " ( substitutions: " << sub << " deletions: " << del << " insertions: " << ins << " )" << std::endl;
        lastLL = testLL;
      }

    }

    std::cout << "Finish test examples: " << num_examples << " examples; logLikelihood: " << testLL << std::endl;
    double PER = perr;
    PER = PER * 100.0 / pref;
    std::cout << "PER " << PER << "% phone errors: " << perr << " reference phones: " << pref << " ( substitutions: " << sub << " deletions: " << del << " insertions: " << ins << " )" << std::endl;
  }
}
