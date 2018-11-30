#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>
#include <pthread.h>

#include <fst/fst.h>
#include <fst/fst-decl.h>
#include <fst/fstlib.h>
#include <fst/matcher.h>
#include <fst/compose-filter.h>
#include <fst/minimize.h>
#include <beam-search.h>

#include "HPYLMFst.hpp"
#include "hpylm_made_usefull.h"

#include <boost/program_options.hpp>

namespace po = boost::program_options;

using fst::LogArc;
typedef fst::VectorFst<fst::LogArc> LogVectorFst;
typedef LogArc::StateId StateId;

template <typename T>
void write_debug_fst(const T& fst, const std::string &filename, const int beam=-1) {
  std::cerr << "About to write " << filename << std::endl;
  LogVectorFst expanded(fst);
  fst::StdVectorFst tmp;
  fst::Cast(expanded, &tmp);
  if (beam > 0) {
    std::cerr << "Writing debug (beam=" << beam << ") FST into " << filename;
    fst::Prune(&tmp, beam);
    std::cerr << " ...done" << std::endl;
  } else {
    std::cerr << "Writing debug FST into " << filename << std::endl;
  }
  //fst::Connect(&tmp);
  tmp.Write(filename);
}

template <typename T>
void prune_fst(const T& fst, LogVectorFst *ret, const int beam=5) {
  LogVectorFst expanded(fst);
  fst::StdVectorFst tmp;
  fst::Cast(expanded, &tmp);
  fst::Prune(&tmp, beam);
  fst::Cast(tmp, ret);
}

template <typename T>
void prune_fst_using_nbest(const T& fst, LogVectorFst *ret, const int nbest=10000) {
  LogVectorFst expanded(fst);
  fst::StdVectorFst tmp, out;
  fst::Cast(expanded, &tmp);
  fst::ShortestPath(tmp, &out, nbest);
  fst::Cast(tmp, ret);
}

template <typename WordBaseDist>
std::shared_ptr<HPYLM> CreateG2PModel(const int lm_order,
    const vector<int> wordlist,
    const WordBaseDist &dist) {
  std::shared_ptr<HPYLM> hpylm = std::make_shared<HPYLM>(lm_order);

  return hpylm;
}

std::vector<int> CreateWordlist(WordMap &symbols) {
  int words = symbols.numWords();
  std::vector<int> ret(words-2, 0); // minus <eps> and <phi>

  if ((symbols["<eps>"] != 0) || (symbols["<phi>"] != 1)) {
    std::cerr << "<eps> must be index 0 and <phi> must be index 1!" << std::endl;
    exit(1);
    return std::vector<int>();
  }
  for (int i = 2; i < words; ++i) {
    std::wstring word = symbols[i];
    if (word.empty()) {
      std::cout << "Word ID does not exist. Symbol table is probably not continuous" << std::endl;
    }
    ret.at(i-2) = i;
  }
  return ret;
}

auto CreateBaseDist(std::vector<int> wordlist) {
  int words = wordlist.size();
  google::dense_hash_map<int, double> base_dist;
  base_dist.set_empty_key(EMPTY);
  base_dist.set_deleted_key(DELETED);

  for (int i = 0; i < words; ++i) {
    int word = wordlist[i];
    base_dist.insert(std::make_pair(word, 1./words));
  }
  return base_dist;
}

std::shared_ptr<LogVectorFst> LoadFst(const std::string &s) {
  std::shared_ptr<LogVectorFst> ret(LogVectorFst::Read(s.c_str()));
  if (ret.get() == nullptr) {
    std::cout << "Verify " << s << " : Failed to load" << std::endl;
    exit(1);
  } else if (fst::Verify(*ret)) {
    std::cout << "Verify " << s << " : OK" << std::endl;
    std::cout << "Num of " << s << " states: " << ret->NumStates() << std::endl;
    std::cout << "Num of " << s << " arcs: " << ret->NumArcs(ret->Start()) << std::endl;
    //fst::Minimize(ret.get(), (LogVectorFst *)(NULL), fst::kDelta, true);
    //std::cout << "Num of " << s << " states: " << ret->NumStates() << " after min"<< std::endl;
    //std::cout << "Num of " << s << " arcs: " << ret->NumArcs(ret->Start()) << " after min" << std::endl;
  } else {
    std::cerr << "Verify " << s << ": Fail" << std::endl;
    exit(1);
  }
  return ret;
}

template<class A, class B>
auto LazyComposeProjectSort(A &a, B &b) {
  typedef fst::OLabelCompare<LogArc> SortType;
  fst::ComposeFst<LogArc> composed(a, b);
  fst::ProjectFst<LogArc> projected(composed, fst::PROJECT_OUTPUT);
  fst::ArcSortFst<LogArc, SortType> olabel_sorted(projected, SortType());
  return olabel_sorted;
}

template<class A, class B>
auto LazyComposeSort(A &a, B &b) {
  typedef fst::OLabelCompare<LogArc> SortType;
  fst::ComposeFst<LogArc> composed(a, b);
  fst::ArcSortFst<LogArc, SortType> olabel_sorted(composed, SortType());
  return olabel_sorted;
}

template<class A, class B>
auto LazyPhiComposeSort(A &a, B &b) {
  typedef fst::OLabelCompare<LogArc> SortType;
  fst::ComposeFst<LogArc> composed = PhiCompose(a, b, PHI_SYMBOLID);
  fst::ArcSortFst<LogArc, SortType> olabel_sorted(composed, SortType());
  return olabel_sorted;
}

auto SamplePath(LogVectorFst &fst, const int samples, const int seed) {
  typedef fst::LogProbArcSelector<LogArc> MySelector;
  fst::RandGenOptions<MySelector> opts(MySelector(seed),
      std::numeric_limits<int32>::max(),
      samples, //number of samples
      false,   //weighted tree, does not matter for n=1
      true);   //remove total weight, needed for num stability

  LogVectorFst sampled_path;
  fst::RandGen(fst, &sampled_path, opts);
  return sampled_path;
}

auto BestPath(LogVectorFst &fst) {
  LogVectorFst best_path_logsemiring;
  fst::StdVectorFst best_path, tmp_lattice;
  fst::Cast(fst, &tmp_lattice);
  fst::ShortestPath(tmp_lattice, &best_path);
  fst::Cast(best_path, &best_path_logsemiring);
  return best_path_logsemiring;
}

long int tick() {
  using namespace std::chrono;
  long int ms = duration_cast< std::chrono::milliseconds >(
      steady_clock::now().time_since_epoch()
      ).count();
  return ms;
}


pthread_mutex_t mutexLock = PTHREAD_MUTEX_INITIALIZER;
struct OneSentencePackage {
    std::shared_ptr<LogVectorFst> p2g_fst;
    std::shared_ptr<LogVectorFst> g2l_fst;
    std::shared_ptr<LogVectorFst> l_fst;
    std::shared_ptr<HPYLMFst>  g2g_lm_fst;
    std::vector<int> sentence;
    LogVectorFst sentence_fst;
    LogVectorFst output;
    std::shared_ptr<LogVectorFst> word_lm_fst;
    std::string id;
    int beam_size;
    
    static void *process(void *ptr) {
        ((OneSentencePackage *)ptr)->run();
        return nullptr;
    }

    void run() {
        pthread_mutex_lock( &mutexLock );
        auto p_p2g = LazyComposeProjectSort(sentence_fst, *p2g_fst);
        //cout << "Composed P and P2G for sentence" << id << " itr " <<  std::endl; 
        auto p_p2g_g2g = LazyPhiComposeSort(p_p2g, *g2g_lm_fst);
        //cout << "Composed P_P2G and G2G for sentence " << id << " itr " <<  std::endl; 
        auto p_p2g_g2g_g2l = LazyComposeSort(p_p2g_g2g, *g2l_fst);
        //cout << "Composed P_P2G_G2G and G2L for sentence " << id << " itr " <<  std::endl; 
        //cout << "Composed P_P2G_G2G_G2L and L for sentence " << id << " itr " <<  std::endl; 
        auto complete_fst = LazyComposeSort(p_p2g_g2g_g2l, *word_lm_fst); // leads to Segmentation fault
        pthread_mutex_unlock( &mutexLock );
        //cout << "Composed P_P2G_G2G_G2L_L and W for sentence " << id << " itr " <<  std::endl; 
        sentence_fst.Write(id + ".p_p2g.fst");
        LogVectorFst(p_p2g).Write(id + ".p_p2g.fst");
        LogVectorFst(p_p2g_g2g).Write(id + ".p_p2g_g2g.fst");
        LogVectorFst(p_p2g_g2g_g2l).Write(id + ".p_p2g_g2g_g2l.fst");
        LogVectorFst(p_p2g_g2g_g2l).Write(id + ".p_p2g_g2g_g2l.fst");
        LogVectorFst(complete_fst).Write(id + ".complete_thread.fst");
        LogVectorFst final_fst;
        BeamTrim(complete_fst, &final_fst, beam_size);
        fst::Connect(&final_fst);
        final_fst.Write(id + ".trimmed_thread.fst");
        //cout << "Beam trimmed for sentence " << id << " itr " <<  std::endl; 
        fst::Push<LogArc, fst::REWEIGHT_TO_INITIAL>(final_fst,
            &output,
            fst::kPushWeights|fst::kPushRemoveTotalWeight);
        final_fst.Write(id + ".pushed_thread.fst");
        //cout << "Weight pushed for sentence " << id << " itr " <<  std::endl; 
    }
};


int main(int argc, char *argv[]) {
  bool debug = false;

  int text_lm_order = 3;
  int g2g_lm_order = 7;
  int num_thread_max = 20;
  int num_iters_max = 1;
  bool sample = true;
  int beam_size = 400;
  float lm_anti_scale = 1.1;

  std::string name;

  std::string graphemes_syms("data_wsj/train_letters.sym"),
    graphones_syms("data_wsj/train_graphs.sym"),
    phones_syms("data_wsj/train_prons.sym"),
    words_syms("data_wsj/train_words.sym");

  std::string p2g_file("data_wsj/train_p2g.fst"),
    g2l_file("data_wsj/train_g2l.fst"),
    l_file("data_wsj/grapheme_lexicon.fst"),
    word_lm_file("data_wsj/lm3gram.fst"),
    graphone_alignments;

  std::string words_file("data_wsj/train.text"),
    phones_file("data_wsj/train.ali"),
    lm_words_file("data_wsj/train.text"),
    output_words("data_wsj/words.out"),
    output_graphones("data_wsj/graphones.out");


  po::options_description desc("Allowed options");
  desc.add_options()
    ("help", "produce help message")
    ("debug", "enable debugging")
    ("name", po::value<std::string>(&name)->default_value(name),
     "the \"name\" of the experiment -- will be added to some filenames")
    ("text-lm-order", po::value<int>(&text_lm_order)->default_value(text_lm_order),
     "the ngram order of the text LM")
    ("g2g-lm-order", po::value<int>(&g2g_lm_order)->default_value(g2g_lm_order),
     "the ngram order of the G2G LM")
    ("threads", po::value<int>(&num_thread_max)->default_value(num_thread_max),
     "use this amount of threads for training")
    ("beam-size", po::value<int>(&beam_size)->default_value(beam_size),
     "beam_size")
    ("lm-anti-scale", po::value<float>(&lm_anti_scale)->default_value(lm_anti_scale),
     "LM anti-scale (setting this to X will result in scores proportional to weighting LM by 1/X)")
    ("iter", po::value<int>(&num_iters_max)->default_value(num_iters_max),
     "number of iterations")
    ("no-sampling", "do not adapt the G2G LM (just recognize)")
    ("graphemes", po::value<std::string>(&graphemes_syms)->default_value(graphemes_syms),
     "grapheme symbol table")
    ("graphones", po::value<std::string>(&graphones_syms)->default_value("data/graphones.syms"),
     "graphone symbol table")
    ("phones",    po::value<std::string>(&phones_syms)->default_value("data/phones.syms"),
     "phone symbol table")
    ("word-ref",     po::value<std::string>(&words_file)->default_value(words_file),
     "word symbol table")
    ("words",     po::value<std::string>(&words_syms)->default_value("data/words.syms"),
     "word symbol table")
    ("p2g", po::value<std::string>(&p2g_file)->default_value("data/p2g.fst"),
     "phoneme(or AUD unit) to graphone FST")
    ("g2l", po::value<std::string>(&g2l_file)->default_value("data/g2l.fst"),
     "graphone to letter FST")
    ("lexicon",    po::value<std::string>(&l_file)->default_value("data/grapheme_lexicon.fst"),
     "graphemic lexicon")
    ("alignments",    po::value<std::string>(&graphone_alignments)->default_value(graphone_alignments),
     "graphone_alignments to initialize the HPYLM G2G")
    ("word-lm",    po::value<std::string>(&word_lm_file)->default_value(word_lm_file),
     "text to train the word-level LM on")
    ;
  po::options_description hidden("Hidden options");
  hidden.add_options()
    ("input-file", po::value<std::string>(), "input file")
    ("output-words", po::value<std::string>(), "words file")
    ("output-graphones", po::value<std::string>(), "graphone file")
    ;

  po::positional_options_description pos_desc;
  pos_desc.add("input-file", 1);
  pos_desc.add("output-words", 1);
  pos_desc.add("output-graphones", 1);
  po::variables_map vm;
  try {
    po::command_line_parser parser(argc, argv);
    store(parser.options(desc.add(hidden)).positional(pos_desc).run(), vm);
    notify(vm);

    if ( vm.count("help")  ) {
      std::cout << "Themo == automatic (not magic)" << std::endl
        << desc << std::endl;
      return 0;
    }

    if ( vm.count("debug")  ) {
      debug = true;
    }
    if ( vm.count("input-file") ) {
      phones_file = vm["input-file"].as<std::string>();
      cout << "Phone file: " << phones_file << std::endl;
    } else {
      std::cerr << "you need to insert input-file as a first positional argument" << std::endl;
      exit(1);
    }
    if ( vm.count("output-words") ) {
      output_words = vm["output-words"].as<std::string>();
      cout << "Words file: " << output_words << std::endl;
    } else {
      std::cerr << "you need to insert output-words-file as a second positional argument" << std::endl;
      exit(1);
    }
    if ( vm.count("output-graphones") ) {
      output_graphones = vm["output-graphones"].as<std::string>();
      cout << "Graphone file: " << output_graphones << std::endl;
    } else {
      std::cerr << "you need to inserta output-graphones-file as a third positional argument" << std::endl;
      exit(1);
    }

    po::notify(vm); // throws on error, so do after help in case
    // there are any problems     


  } catch(po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    std::cerr << desc << std::endl;
    exit(-1);
  }

  std::ios::sync_with_stdio(false);

  //std::vector<int> start_context(text_m_order - 1, kSOS);
  cerr << graphemes_syms << std::endl;
  cerr << graphones_syms << std::endl;
  cerr << phones_syms << std::endl;
  cerr << words_syms << std::endl;

  WordMap graphemes, graphones, phones, words;
  graphemes.Read(graphemes_syms);
  graphones.Read(graphones_syms);
  phones.Read(phones_syms);
  words.Read(words_syms);

  auto word_strings = ReadFile(words_file, words, words["<w>"]);
  auto phoneme_strings = ReadFile(phones_file);
  std::cerr << "From " << words_file << " read " << word_strings.size()
    << " word strings" << std::endl;
  std::cerr << "From " << phones_file << " read " << phoneme_strings.size()
    << " phoneme strings" << std::endl;

  std::vector<int>  graphone_list = CreateWordlist(graphones);

  auto g2g_basedist = CreateBaseDist(graphone_list);
  
  cout << "Loading P2G " << p2g_file << std::endl;
  auto p2g_fst = LoadFst(p2g_file);
  cout << "Loading G2L " << g2l_file << std::endl;
  auto g2l_fst = LoadFst(g2l_file);
  cout << "Loading L " << l_file << std::endl;
  auto l_fst   = LoadFst(l_file);
  cout << "Creating G2G, order= "<< g2g_lm_order << std::endl;
  auto g2g_lm  = CreateG2PModel(g2g_lm_order, graphone_list, g2g_basedist);
  cout << "Loading WLM " << word_lm_file << std::endl;
  auto word_lm = LoadFst(word_lm_file);

  cout << "Beam: " << beam_size << std::endl;

  std::ofstream words_out, graphones_out;
  words_out.open(output_words);
  graphones_out.open(output_graphones);

  cout << "Loaded Graphones: " << std::endl;
  //std::shared_ptr<LogVectorFst> p2g_fst;
  //std::shared_ptr<LogVectorFst> g2l_fst;
  //std::shared_ptr<LogVectorFst> l_fst;
  std::shared_ptr<HPYLMFst>  g2g_lm_fst;
  //std::shared_ptr<LogVectorFst>  word_lm_fst;
  //word_lm_fst = word_lm;
  std::vector<int> sentence;
  LogVectorFst output;

  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  //pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;

  // If we have an alignment, let's add the sentences. To be correct,
  // let's remember the id of those sequences so that we can remove them
  // if we are gonna recognize them again
  std::unordered_map<std::string, std::vector<int> > recognized_sequences;
  if (!graphone_alignments.empty()) {
    auto alignments = ReadFile(graphone_alignments);
    std::cerr << "Using graphone alignments from "
              << graphone_alignments << std::endl;
    std::cerr << "From the alignments file " << alignments.size() << " read" << std::endl;
    for (auto &ali:alignments) {
      std::string id = ali.first;
      std::vector<int> graphones_alig = ali.second;

      vector<int> ext_graphones(g2g_lm_order - 1, kSOS);
      //ext_graphones.reserve(g2g_lm_order + graphones_alig.size());
      std::copy(graphones_alig.cbegin(), graphones_alig.cend(), std::back_insert_iterator< std::vector<int> >(ext_graphones));
      
      // std::copy(ext_graphones.cbegin(), ext_graphones.cend(),
      //    std::ostream_iterator<int>(std::cerr, " "));
      //cout << std::endl;
      
      //std::wcerr << L" {ALG} " << std::wstring(id.cbegin(), id.cend()) << L" ";
      //PrintFstPathAsText(std::wcerr, graphones_alig, graphones);
      //std::wcerr << std::endl;
      
      for (const_witerator wi = ext_graphones.cbegin() + g2g_lm_order - 1;
          wi != ext_graphones.cend(); ++wi) {
        g2g_lm->AddWord(wi, g2g_basedist[*wi]);
      }
      recognized_sequences[id] = ext_graphones;
    }
    g2g_lm->ResampleHyperParameters();
  }
  
  //LogVectorFst(HPYLMFst(*g2g_lm, graphone_list, vector<int>(g2g_lm_order - 1, kSOS), kEOS, g2g_basedist, std::vector<bool>())).Write("G2G.fst");
  g2g_lm_fst = std::make_shared<HPYLMFst>(*g2g_lm, graphone_list,
        vector<int>(g2g_lm_order - 1, kSOS),
        kEOS, g2g_basedist, std::vector<bool>());

  std::vector<std::string> keys;
  keys.reserve(phoneme_strings.size());
  std::transform(phoneme_strings.begin(), phoneme_strings.end(),
                std::back_inserter(keys),
                [](const auto &pair){return pair.first;});
  std::sort(keys.begin(), keys.end(),
            [&](const auto &a, const auto &b){
              return phoneme_strings.at(b).size() > phoneme_strings.at(a).size();
              }
          );

        std::vector<OneSentencePackage> packages(num_thread_max); 
        for (int i = 0; i < num_thread_max; ++i) {
            packages[i].word_lm_fst = word_lm;
            packages[i].g2l_fst = g2l_fst;
            packages[i].p2g_fst = p2g_fst;
            packages[i].l_fst = l_fst;
            packages[i].g2g_lm_fst = g2g_lm_fst;
            cout << "Fed wordLM into thread" << std::endl;
        }

  for(int sampling_iteration = 0; sampling_iteration < num_iters_max; ++sampling_iteration) {
    auto strings_iterator = keys.cbegin();
    int minibatch=0;
    while (strings_iterator != keys.cend()) {
            int threads_to_run = 0;
            while ((threads_to_run < num_thread_max) && (strings_iterator != keys.end())) {
                std::string id = *strings_iterator;;

                std::vector<int> sentence(phoneme_strings.at(id));
                sentence.push_back(kEOS);

                LogVectorFst sentence_fst;
                create_linear_transducer(sentence, &sentence_fst);
                //sentence_fst.Write(id + ".fst");
                //g2g_lm_fst.Write(id + ".g2g.fst");

                packages[threads_to_run].sentence_fst = sentence_fst;
                packages[threads_to_run].sentence = sentence;
                packages[threads_to_run].id = id;
                packages[threads_to_run].beam_size = beam_size + 10 * sentence.size();
                threads_to_run++; 
                strings_iterator++;
            }

        minibatch += threads_to_run;
        cout << "Finished feeding the packages into threads" << std::endl; 

 //**********************************************************************************
    // Multi threading code is here
      std::cerr << "Running threads...[" ;
      std::vector<pthread_t> threads(threads_to_run);
      std::cerr << "Initialized threads...[" ;
      for(int task_id = 0; task_id < threads_to_run; ++task_id) {
        cout << task_id << " ";
        int rc = pthread_create(&threads[task_id], &attr,
            OneSentencePackage::process,
            (void *)&packages[task_id] );
        if (rc){
          cout << "Error:unable to create thread," << rc << endl;
          exit(-1);
        }
      }
      cout <<  "] " << std::endl;

      // Wait for all the threads

      //pthread_mutex_lock( &mutex1 );
      cout << "Waiting for the running threads" << std::endl;
      //pthread_mutex_unlock( &mutex1 );
      auto ticks = tick();
      for(int task_id = 0; task_id < threads_to_run; ++task_id) {
        void *status;
        int rc = pthread_join(threads[task_id], &status);
        if (rc){
          cout << "Error:unable to join," << rc << endl;
          exit(-1);
        }
      }
      cout << "Waited " << (tick() - ticks)/1000.0 << "seconds" << std::endl;
 //********************************************************************************

        for(int task_id = 0; task_id < threads_to_run; ++task_id) {
            LogVectorFst out = packages[task_id].output;
            std::string id = packages[task_id].id;
            std::vector<int> sentence = packages[task_id].sentence;

            vector<int> word_symbols;
            vector<int> graphone_symbols;

            LogArc::Weight score;
            LogVectorFst sampled_path = SamplePath(out, 1, 1);
            LogVectorFst best_path = BestPath(out);
            GetLinearSymbolSequence(best_path, &graphone_symbols, &word_symbols, &score);
            std::wcerr << L" {REF} " << std::wstring(id.cbegin(), id.cend()) << L" ";
            PrintFstPathAsText(std::wcerr, word_strings[id], words);
            std::wcerr << std::endl;
            std::wcerr << L" {REF} " << std::wstring(id.cbegin(), id.cend()) << L" ";
            PrintFstPathAsText(std::wcerr, sentence, phones);
            std::wcerr << std::endl;
            std::wcerr << L" Score of the best path: " << score.Value() << std::endl;
            words_out << id << " ";
            std::copy(word_symbols.cbegin(), word_symbols.cend()-1, std::ostream_iterator<int>(words_out, " "));
            words_out << std::endl;
            graphones_out << id << " ";
            std::copy(graphone_symbols.cbegin(), graphone_symbols.cend()-1, std::ostream_iterator<int>(graphones_out, " "));
            graphones_out << std::endl;
            std::wcerr << L" Best path graphone ";
            PrintFstPathAsText(std::wcerr, graphone_symbols, graphones);
            std::wcerr << std::endl;
            std::wcerr << " Best path words: ";
            PrintFstPathAsText(std::wcerr, word_symbols, words);
            std::wcerr << std::endl;
            
            if (sample) {
                graphone_symbols.clear();
                word_symbols.clear();
                GetLinearSymbolSequence(sampled_path, &graphone_symbols, &word_symbols, &score);
                std::wcerr << L" Score of the sampled path:" << score.Value()
                << L" "  << graphone_symbols.size()
                << L" "  << word_symbols.size() << std::endl;
             vector<int> extended_graphones(g2g_lm_order - 1, kSOS);
                std::copy(graphone_symbols.cbegin(), graphone_symbols.cend(), std::back_insert_iterator< std::vector<int> >(extended_graphones));
                std::wcerr << L" Sampled path graphone ";
                PrintFstPathAsText(std::wcerr, graphone_symbols, graphones);
                std::wcerr << std::endl;
                std::wcerr << " Sampled path words: ";
                PrintFstPathAsText(std::wcerr, word_symbols, words);
                std::wcerr << std::endl;
                if (recognized_sequences.find(id) != recognized_sequences.end()) {
                    for (const_witerator wi = recognized_sequences[id].cbegin() + g2g_lm_order - 1;
                        wi != recognized_sequences[id].cend(); ++wi) {
                        g2g_lm->RemoveWord(wi);
                    }   
                }
                for (const_witerator wi = extended_graphones.cbegin() + g2g_lm_order - 1;
                    wi != extended_graphones.cend(); ++wi) {
                    cout << *wi << " ";
                    g2g_lm->AddWord(wi, g2g_basedist[*wi]);
                }
                cout << std::endl;
                recognized_sequences[id] = extended_graphones;
            }
        }
      for (int i = 0; i < num_thread_max; ++i) {
        packages[i].g2g_lm_fst = std::make_shared<HPYLMFst>(*g2g_lm, graphone_list,
            vector<int>(g2g_lm_order - 1, kSOS),
            kEOS, g2g_basedist, std::vector<bool>());
      }
      LogVectorFst expanded(*packages[0].g2g_lm_fst);
      if (name == "") {
        expanded.Write("g2g.fst." + std::to_string(sampling_iteration)  + "." + std::to_string(minibatch));
      } else {
        expanded.Write("g2g." + name + ".fst." + std::to_string(sampling_iteration) + "." + std::to_string(minibatch));
      }
    }

    g2g_lm->ResampleHyperParameters();
    for (int i = 0; i < num_thread_max; ++i) {
      packages[i].g2g_lm_fst = std::make_shared<HPYLMFst>(*g2g_lm, graphone_list,
          vector<int>(g2g_lm_order - 1, kSOS),
          kEOS, g2g_basedist, std::vector<bool>());
    }
    LogVectorFst expanded(*packages[0].g2g_lm_fst);
    if (name == "") {
      expanded.Write("g2g.fst." + std::to_string(sampling_iteration) );
    } else {
      expanded.Write("g2g." + name + ".fst." + std::to_string(sampling_iteration) );
    }
  }
  return 0;
}