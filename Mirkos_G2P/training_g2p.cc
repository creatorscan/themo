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
#include "HPYLMFst.hpp"

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
  //kSOW = 2,
  //kEOW = 3,
  ///kSOS = 215,
  //kEOS = 215
} SpecialWordsType;


//const int lm_order = 7;
//std::vector<int> start_context(lm_order - 1, kSOS);
//std::vector<int> start_context(lm_order - 1);

template <class Arc>
ComposeFst<Arc> RPhiCompose(const Fst<Arc>& fst1, const Fst<Arc>& fst2, typename Arc::Label phi_label) {
  typedef PhiMatcher<Matcher<Fst<Arc> > > PM;
  ComposeFstOptions<Arc, PM> copts(CacheOptions(), new PM(fst1, MATCH_NONE, kNoLabel), new PM(fst2, MATCH_INPUT, phi_label));
  return ComposeFst<Arc>(fst1, fst2, copts);
};

template<class Arc>
fst::ComposeFst<Arc> PhiCompose(const fst::Fst<Arc> &fst1, const fst::Fst<Arc> &fst2, typename Arc::Label phi_label) {
  typedef fst::SortedMatcher<fst::Fst<Arc> > SM;
  typedef fst::PhiMatcher<SM>                PM;
  fst::CacheOptions base_opts;
  base_opts.gc_limit = 0; // Cache only the last state for fastest copy.
  fst::ComposeFstImplOptions<SM, PM> impl_opts(base_opts);
  // These pointers are taken ownership of, by ComposeFst.
  impl_opts.matcher1 = new SM(fst1, fst::MATCH_NONE);
  impl_opts.matcher2 = new PM(fst2, fst::MATCH_INPUT, phi_label, false);
  return fst::ComposeFst<Arc>(fst1, fst2, impl_opts);
}

int main(int argc, char *argv[] ) {
  //srand (time(NULL));
  srand(1234);
  const float heldout_percentage = 0.05;

  const char *far_phone_name = argv[1]; //"data_aud/train_prons.far";
  const char *far_graph_name = argv[2]; //"data_aud/train_letters.far";
  const char *fst_p2g_name = argv[3]; //"data_aud/train_p2g.fst";
  const char *fst_g2l_name = argv[4]; //"data_aud/train_g2l.fst";
  const char *fst_e_lim_name = argv[5]; //"data_aud/train_limiter.fst";
  const char *EOS_value = argv[6];
  int kEOS = atoi(EOS_value);
  int kSOS = kEOS;

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
  //int np = 0, nl = 0, ng = 0, ng2 = 0;
  //int num_graphones = fst_p2g->NumArcs(fst_p2g->Start());
  //Rewrite based on /export/b17/jtrmal/workshop/
const int lm_order = 7;
std::vector<int> start_context(lm_order - 1, kSOS);


  HPYLM hpylm(lm_order);

  vector<int> word_list;
  google::dense_hash_map<int, double> base_dist;
  base_dist.set_empty_key(EMPTY);
  base_dist.set_deleted_key(DELETED);

  for (ArcIterator<Fst<MyArc>> aiter(*fst_p2g, fst_p2g->Start()); !aiter.Done(); aiter.Next()) {
    int w = aiter.Value().olabel;
    word_list.push_back(w);
    base_dist.insert(std::make_pair(w, 1./fst_p2g->NumArcs(fst_p2g->Start())));
    //std::cout << aiter.Value().olabel << std::endl;
  }

  // Indices for held-out set
  std::unordered_set<int> heldout_set;
  for (int i = 0; i < num_examples; ++i) {
    if (((double) rand() / (RAND_MAX)) < heldout_percentage) heldout_set.insert(i);
  }
  std::cout << "training examples: " << num_examples -  heldout_set.size() << " heldout: " << heldout_set.size() << std::endl;

  // Create indices for random shuffling
  std::vector<int> indices(num_examples);
  for (int i = 0; i < num_examples; ++i) indices[i] = i;
  // Vector of graphone labels
  vector<vector<int> *> sampled_labels(num_examples, NULL);

  // Initial HPYLM FST
  //HPYLMFst fst_hpylm(hpylm, word_list, start_context, kEOS, base_dist, std::vector<bool>());
  //VectorFst<MyArc> hpylmfst_expanded(fst_hpylm);
  //hpylmfst_expanded.Write("hpylm_initial.fst");

  //##########################################
  //############ Main loop ###################
  //##########################################

  double trainLL = 0.0, heldoutLL = 0.0, last_trainLL = 1.0E7, last_heldoutLL = 1.0E7, train_improvement = 1.0, heldout_improvement = 1.0;
  int iteration = 0;
  while (heldout_improvement > 0.01 && iteration < 100) //train_improvement > 0.01 && 
  {
    std::random_shuffle(indices.begin(), indices.end());
    //trainLL = 0.0;
    //double lastLL = 0.0;
    heldoutLL = 0.0;
    int training_examples = 0;

    for(int i = 0; i < num_examples; i++) {
      int ind = indices[i];
      bool heldout = false;
      //std::unordered_set<int>::const_iterator it = heldout_set.find(ind);
      if (heldout_set.find(ind) != heldout_set.end()) heldout = true;
      if (!heldout) training_examples++;

      std::cerr << "eg:index > " << ind << std::endl;
      // Get phone and letter strings
      auto lex_entry = lexicon[ind];
      //HPYLMFst fst_hpylm(hpylm, word_list, start_context, kEOS, base_dist, std::vector<bool>());
      HPYLMFst fst_hpylm(hpylm, word_list, start_context, kEOS, base_dist, std::vector<bool>());
      VectorFst<MyArc> hpylmfst_expanded_out(fst_hpylm);
      //hpylmfst_expanded_out.Write("hpylm_init.fst");

      VectorFst<MyArc> fst_in2g;
      fst::Compose(*lex_entry.second, *fst_p2g, &fst_in2g);
      fst::Project(&fst_in2g, PROJECT_OUTPUT);
      //fst_in2g.Write("fst_in2g_" + std::to_string(ind) + ".fst");
      VectorFst<MyArc> fst_in2g_lim;
      fst::Compose(fst_in2g, *fst_lim, &fst_in2g_lim);
      VectorFst<MyArc> fst_in2l;
      fst::Compose(fst_in2g_lim, *fst_g2l, &fst_in2l);
      //fst_in2l.Write("fst_in2l_" + std::to_string(ind) + ".fst");
      //lex_entry.first->Write("fst_lex_phones_" + std::to_string(ind) + ".fst");
      //lex_entry.second->Write("fst_lex_letters_" + std::to_string(ind) + ".fst");
      VectorFst<MyArc> fst_in2out;
      fst::Compose(fst_in2l, *lex_entry.first, &fst_in2out);
      fst::Project(&fst_in2out, PROJECT_INPUT);
      ArcSort(&fst_in2out, OLabelCompare<MyArc>());
      //fst_in2out.Write("fst_in2out_" + std::to_string(ind) + ".fst");
      ComposeFst<MyArc> fst_in2out_lm = PhiCompose(fst_in2out, fst_hpylm, kPHI);
      //VectorFst<MyArc> fst_out;
      //fst::Compose(fst_in2out, fst_hpylm, &fst_out);
      VectorFst<MyArc> fst_out(fst_in2out_lm);
      //fst_out.Write("fst_out_" + std::to_string(ind) + ".fst");
/*
      ComposeFst<MyArc>                      fst_in2g     (*lex_entry.second, *fst_p2g);
      fst::ProjectFst<MyArc>                 fst_in2g_proj(fst_in2g,          PROJECT_OUTPUT);
      //ArcSortFst<MyArc,OLabelCompare<MyArc>> fst_in2g_sort(fst_in2g_proj,      OLabelCompare<MyArc>());
      //ComposeFst<MyArc>                      fst_in2g_lim (fst_in2g_proj,     *fst_lim);
      //ArcSortFst<MyArc,OLabelCompare<MyArc>> fst_in2g_lisr(fst_in2g_lim,      OLabelCompare<MyArc>()); 
      ComposeFst<MyArc>           fst_in2g_lm = PhiCompose(fst_in2g_proj,     fst_hpylm, kPHI);
      ComposeFst<MyArc>                      fst_in2l     (fst_in2g_lm,       *fst_g2l);
      ComposeFst<MyArc>                      fst_in2out   (fst_in2l,          *lex_entry.first);
      VectorFst<MyArc> fst_out(fst_in2out);
*/
      fst::Connect(&fst_out);

      //fst_out.Write("fst_out_afterCOnn_" + std::to_string(ind) + ".fst");
      if (i == -1) {
        lex_entry.second->Write("fst_in.fst");
        lex_entry.first->Write("fst_out.fst");
        fst_in2g.Write("fst_in2g.fst");
        fst_in2g_lim.Write("fst_in2g_lim.fst");
        fst_in2l.Write("fst_in2l.fst");
        fst_in2g.Write("fst_in2g.fst");
        fst_in2out.Write("fst_in2out.fst");
        fst_out.Write("fst_in2out_lm.fst");
        //sampled_path.Write("sampled_path.fst");
        HPYLMFst fst_hpylm_out(hpylm, word_list, start_context, kEOS, base_dist, std::vector<bool>());
        VectorFst<MyArc> hpylmfst_expanded_out(fst_hpylm_out);
        hpylmfst_expanded_out.Write("hpylm.fst");
        //exit(0);
      }

      if (fst_out.NumStates() == 0) {
        std::cerr << "WARNING: Empty composition in line:" << ind << std::endl;
        continue;
      }

      if (heldout) {
        vector<MyArc::Weight> distance;
        fst::ShortestDistance(fst_out, &distance, fst::REWEIGHT_TO_FINAL);
        assert(fst_out.Start() < distance.size());
        MyArc::Weight total_weight = distance[fst_out.Start()];
        //std::cout << "Lattice total weight:" << total_weight.Value() << std::endl;
        heldoutLL += total_weight.Value();
      } else { // Sample path and train the model
        //trainLL += total_weight.Value();

        // Weight pushing
        VectorFst<MyArc> fst_pushed;
  ////  LogVectorFst pruned_fst;
  ////  prune_fst(*complete_fst, &pruned_fst, 2);
        fst::Push<MyArc, fst::REWEIGHT_TO_INITIAL>(fst_out, &fst_pushed, 
                                                 fst::kPushWeights | fst::kPushRemoveTotalWeight);
         // Sample:
        VectorFst<MyArc> sampled_path;
        int seed = 1;
        int samples = 1;
        typedef fst::LogProbArcSelector<MyArc> MySelector;
        fst::RandGenOptions<MySelector> opts(MySelector(seed),
                                            std::numeric_limits<int32>::max(),
                                            samples, //number of samples
                                            false,   //weighted tree, does not matter for n=1
                                            true);   //remove total weight, needed for num stability
        fst::RandGen(fst_pushed, &sampled_path, opts);
        assert(sampled_path.NumStates() > 0);
        //  std::cerr << "WARNING: Empty sampled path:" << ind << std::endl;
        //  continue;

        Fst<MyArc>::StateId state_id = sampled_path.Start();
        Fst<MyArc>::StateId dest_state;
        vector<int>* label_seq = new vector<int>(lm_order-1, kSOS);
        while(sampled_path.Final(state_id) == Fst<MyArc>::Arc::Weight::Zero()) { 
          ArcIterator<Fst<MyArc>> aiter(sampled_path, state_id);
          dest_state = aiter.Value().nextstate;
          assert(dest_state != fst::kNoStateId);
          state_id = dest_state;
          label_seq->push_back(aiter.Value().ilabel);
          //std::cout << " " << aiter.Value().ilabel;
        }
        //std::cout << std::endl;

        // Remove words from LM
        if (iteration > 0) {
          for (const_witerator wi = sampled_labels[ind]->begin() + 
               lm_order - 1; wi != sampled_labels[ind]->end(); ++wi) {
            //std::cerr << *wi << std::endl;
            hpylm.RemoveWord(wi);
          }
          delete sampled_labels[ind];
        }

        // Update LM: hpylm.AddWordSequenceToLm(sentence);
        sampled_labels[ind] = label_seq;
        for (const_witerator wi = label_seq->begin() + 
              lm_order - 1; wi != label_seq->end(); ++wi) {
          //std::cerr << *wi << std::endl;
          hpylm.AddWord(wi, base_dist[*wi-2]);
        }
      }

      if (!heldout && (training_examples % 50 == 49)) {
        std::cout << "sampling " << training_examples + 1 << std::endl;
        //std::cout << "sampling " << training_examples + 1 << " " << trainLL - lastLL << std::endl;
        //lastLL = trainLL;
      }
      if (!heldout && (training_examples % 15000 == 14999)) {
        hpylm.ResampleHyperParameters();
        std::cout << "resample hyperparameters" << std::endl;
      }

    } // Finish epoch (iteration)

    //train_improvement = abs(last_trainLL / trainLL) - 1.0;
    heldout_improvement = abs(last_heldoutLL / heldoutLL) - 1.0;
    last_heldoutLL = heldoutLL;
    //last_trainLL = trainLL;
    //std::cout << "Finish iteration " << iteration << " with " << training_examples << " examples; logLikelihood: " << trainLL 
    //  << " improvement: " << train_improvement << " heldout: " << heldoutLL << " improvement: " << heldout_improvement << std::endl;
    std::cout << "Finish iteration " << iteration << " with " << training_examples << " examples; heldout: " << heldoutLL << " improvement: " << heldout_improvement << std::endl;
    HPYLMFst fst_hpylm2(hpylm, word_list, start_context, kEOS, base_dist, std::vector<bool>());
    VectorFst<MyArc> hpylmfst_expanded2(fst_hpylm2);
    hpylmfst_expanded2.Write("data_bhargav/hpylm.iteration"+std::to_string(iteration)+".fst");
    iteration++;
    if (training_examples < 15000) {
      hpylm.ResampleHyperParameters();
      std::cout << "resample hyperparameters" << std::endl;
    }
  }

  for(auto label_seq: sampled_labels) {
    delete label_seq;
  }

  // fold-back training on held-out examples
  heldoutLL = 0.0;
  for ( auto it = heldout_set.begin(); it != heldout_set.end(); ++it )
  {
    int ind = *it;
    auto lex_entry = lexicon[ind];
    HPYLMFst fst_hpylm(hpylm, word_list, start_context, kEOS, base_dist, std::vector<bool>());

    VectorFst<MyArc> fst_in2g;
    fst::Compose(*lex_entry.second, *fst_p2g, &fst_in2g);
    fst::Project(&fst_in2g, PROJECT_OUTPUT);
    VectorFst<MyArc> fst_in2l;
    fst::Compose(fst_in2g, *fst_g2l, &fst_in2l);
    VectorFst<MyArc> fst_in2out;
    fst::Compose(fst_in2l, *lex_entry.first, &fst_in2out);
    fst::Project(&fst_in2out, PROJECT_INPUT);
    ArcSort(&fst_in2out, OLabelCompare<MyArc>());
    ComposeFst<MyArc> fst_in2out_lm = PhiCompose(fst_in2out, fst_hpylm, kPHI);
    VectorFst<MyArc> fst_out(fst_in2out_lm);

    fst::Connect(&fst_out);
    if (fst_out.NumStates() == 0) {
      std::cerr << "WARNING: Empty composition in line:" << ind << std::endl;
      continue;
    }

    vector<MyArc::Weight> distance;
    fst::ShortestDistance(fst_out, &distance, fst::REWEIGHT_TO_FINAL);
    assert(fst_out.Start() < distance.size());
    MyArc::Weight total_weight = distance[fst_out.Start()];
    //std::cout << "Lattice total weight:" << total_weight.Value() << std::endl;
    heldoutLL += total_weight.Value();

    // Weight pushing
    VectorFst<MyArc> fst_pushed;
    fst::Push<MyArc, fst::REWEIGHT_TO_INITIAL>(fst_out, &fst_pushed, 
                                                 fst::kPushWeights | fst::kPushRemoveTotalWeight);
    // Sample:
    VectorFst<MyArc> sampled_path;
    int seed = 1;
    int samples = 1;
    typedef fst::LogProbArcSelector<MyArc> MySelector;
    fst::RandGenOptions<MySelector> opts(MySelector(seed),
                                        std::numeric_limits<int32>::max(),
                                        samples, //number of samples
                                        false,   //weighted tree, does not matter for n=1
                                        true);   //remove total weight, needed for num stability
    fst::RandGen(fst_pushed, &sampled_path, opts);
    assert(sampled_path.NumStates() > 0);

    Fst<MyArc>::StateId state_id = sampled_path.Start();
    Fst<MyArc>::StateId dest_state;
    vector<int>* label_seq = new vector<int>(lm_order-1, kSOS);
    while(sampled_path.Final(state_id) == Fst<MyArc>::Arc::Weight::Zero()) { 
      ArcIterator<Fst<MyArc>> aiter(sampled_path, state_id);
      dest_state = aiter.Value().nextstate;
      assert(dest_state != fst::kNoStateId);
      state_id = dest_state;
      label_seq->push_back(aiter.Value().ilabel);
    }

    // Update LM: hpylm.AddWordSequenceToLm(sentence);
    for (const_witerator wi = label_seq->begin() + lm_order - 1; 
          wi != label_seq->end(); ++wi) {
      hpylm.AddWord(wi, base_dist[*wi-2]);
    }

  }
  heldout_improvement = abs(last_heldoutLL / heldoutLL) - 1.0;
  std::cout << "Fold-back iteration " << iteration << " with " << heldout_set.size() << " examples; heldout logLikelihood: " 
    << heldoutLL << " improvement: " << heldout_improvement << std::endl;
  HPYLMFst fst_hpylm2(hpylm, word_list, start_context, kEOS, base_dist, std::vector<bool>());
  VectorFst<MyArc> hpylmfst_expanded2(fst_hpylm2);
  //hpylmfst_expanded2.Write("hpylm.iteration"+std::to_string(iteration)+".fst");
  hpylmfst_expanded2.Write("data_bhargav/hpylm.final.fst");
}
