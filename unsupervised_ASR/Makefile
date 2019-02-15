SHELL := /bin/bash
#CXXFLAGS := -g3 -O3  -fpermissive -I./NHPYLM/ext_deps/ -I./NHPYLM/ -I./submodule/LatticeWordSeg/tools/include/ $(CXXFLAGS) 
openfst=openfst
boost=../boost_1_61_0
nhpylm=require/NHPYLM
kaldi=/mnt/matylda6/baskar/Kaldi/kaldi/src
CXXFLAGS :=  -ggdb -fpermissive -I./$(nhpylm)/ext_deps/ -I./$(nhpylm)/ -I$(kaldi)/ -I./$(openfst)/src/include -I./$(boost) -std=c++14 $(CXXFLAGS)

#CXX = clang++
CXX := g++-6.4
CC := gcc-6.4

.PHONY: clean test wsj babel

all: hpylmlib.a  test_compose

hpylmlib.a: $(nhpylm)/Dictionary.o $(nhpylm)/NHPYLM.o $(nhpylm)/HPYLM.o $(nhpylm)/Restaurant.o 
	ar cru $@ $? 

fstphicompose: fstphicompose.o  hpylm_made_usefull.o hpylmlib.a
	$(CXX) $(CXXFLAGS)  -L$(PWD)/../openfst-1.5.3/src/lib/.libs -L$(PWD)/../boost_1_61_0/stage/lib -Wl,-rpath=$(PWD)/../openfst-1.5.3/src/lib/.libs,-rpath=$(PWD)/../boost_1_61_0/stage/lib -lfst $^ -o $@

test_hpylm: test_hpylm.o  hpylm_made_usefull.o hpylmlib.a

themo-align: themo-align.o hpylm_made_usefull.o HPYLMFst.o hpylmlib.a
	$(CXX) $(CXXFLAGS)  -L$(PWD)/../openfst-1.5.3/src/lib/.libs -L$(PWD)/../boost_1_61_0/stage/lib -Wl,-rpath=$(PWD)/../openfst-1.5.3/src/lib/.libs,-rpath=$(PWD)/../boost_1_61_0/stage/lib -lboost_program_options -lfst $^ -o $@

themo-lm: themo-lm.o hpylm_made_usefull.o HPYLMFst.o hpylmlib.a
	$(CXX) $(CXXFLAGS)  -L$(PWD)/../openfst-1.5.3/src/lib/.libs -L$(PWD)/../boost_1_61_0/stage/lib -Wl,-rpath=$(PWD)/../openfst-1.5.3/src/lib/.libs,-rpath=$(PWD)/../boost_1_61_0/stage/lib -lboost_program_options -lfst $^ -o $@

themo: themo.o hpylm_made_usefull.o HPYLMFst.o hpylmlib.a
	$(CXX) $(CXXFLAGS)  -L$(PWD)/$(openfst)/src/lib/.libs -L$(PWD)/$(boost)/stage/lib -Wl,-rpath=$(PWD)/$(openfst)/src/lib/.libs,-rpath=$(PWD)/$(boost)/stage/lib -lboost_program_options -lfst -lpthread $^ -o $@

test_compose: test_compose.o hpylm_made_usefull.o NHPYLMFst.o hpylmlib.a
	$(CXX) $(CXXFLAGS)  -L$(PWD)/../openfst-1.5.3/src/lib/.libs -L$(PWD)/../boost_1_61_0/stage/lib -Wl,-rpath=$(PWD)/../openfst-1.5.3/src/lib/.libs,-rpath=$(PWD)/../boost_1_61_0/stage/lib -lfst $^ -o $@

test_hpylmfst: test_hpylmfst.o hpylm_made_usefull.o NHPYLMFst.o hpylmlib.a
	$(CXX) $(CXXFLAGS)  -L$(PWD)/../openfst-1.5.3/src/lib/.libs -L$(PWD)/../boost_1_61_0/stage/lib -Wl,-rpath=$(PWD)/../openfst-1.5.3/src/lib/.libs,-rpath=$(PWD)/../boost_1_61_0/stage/lib -lfst $^ -o $@

test2: test_hpylmfst
		./test_hpylmfst

wsj: themo test_compose hpylmlib.a
		#-mkdir -p data
		cp  corpus/en_wsj/train.ref  data/train.text
		sed 's/\*//g' corpus/en_wsj/cleaned.training.texts.without_training_unique |sed 's/EXISITING/EXISTING/g' > data/train.text.lm
		./scripts/remap_ids.py corpus/en_wsj/words.txt corpus/en_wsj/specials.syms > data/words.syms
		./scripts/remap_ids.py corpus/en_wsj/phones.syms  corpus/en_wsj/specials.syms > data/phones.syms
		sed 's/SPN/<SPN>/g; s/NSN/<NSN>/g;'  corpus/en_wsj/train.ali | grep -F -f corpus/en_wsj/training_yenda.keys | perl -ne 'chomp; @F = split(" ", $$_, 2); $$F[1] =~ s/[012]//g; print "$$F[0] $$F[1]\n"' | ./scripts/sym2int.pl -f 2- data/phones.syms > data/train.ali
		./scripts/create_grapheme_lexicon2.py --locale en_US --failed-words data/graphemic_lexicon_fails.txt --specials corpus/en_wsj/specials.syms corpus/en_wsj/words.txt  data/grapheme_lexicon.txt data/graphemes.syms
		./scripts/create_g2l_fst.py --specials corpus/en_wsj/specials.syms  data/phones.syms  data/graphemes.syms data/graphones.syms  data/g2l.fst.txt
		./scripts/make_lexicon_fst.pl data/grapheme_lexicon.txt > data/grapheme_lexicon.fst.txt
		./scripts/create_empty_E.py --specials corpus/en_wsj/specials.syms --phones data/phones.syms  --graphemes data/graphemes.syms   data/p2g_primary.fst.txt data/p2g_filter.fst.txt
		fstcompile --arc_type=log --isymbols=data/graphemes.syms --osymbols=data/words.syms data/grapheme_lexicon.fst.txt | fstarcsort --sort_type=ilabel > data/grapheme_lexicon.fst
		fstcompile --arc_type=log --isymbols=data/graphones.syms --osymbols=data/graphemes.syms data/g2l.fst.txt | fstarcsort --sort_type=olabel > data/g2l.fst
		fstcompile --isymbols=data/phones.syms    --osymbols=data/graphones.syms data/p2g_primary.fst.txt | fstencode --encode_labels - data/enc.dat |fstdeterminize |fstminimize |fstencode -decode - data/enc.dat | fstprint |fstcompile --arc_type=log | fstarcsort --sort_type=olabel > data/p2g_primary.fst
		fstcompile --isymbols=data/graphones.syms   --osymbols=data/graphones.syms data/p2g_filter.fst.txt | fstencode --encode_labels - data/enc.dat |fstdeterminize |fstminimize |fstencode -decode - data/enc.dat | fstprint |fstcompile --arc_type=log | fstarcsort --sort_type=olabel > data/p2g_filter.fst
		fstcompose data/p2g_primary.fst data/p2g_filter.fst | fstencode --encode_labels - data/enc.dat |fstdeterminize |fstminimize |fstencode -decode - data/enc.dat | fstprint |fstcompile --arc_type=log | fstarcsort --sort_type=olabel > data/p2g.fst
		cat data/train.ali | awk '{ print length, $$0 }' | sort -n -s | cut -d" " -f2- | head -n 400 | sort > data/train.ali.sub
		cat data/train.ali | awk '{print $$1}' > data/ali_ids
		false
		./themo-mt  --lm-text data/train.text.lm data/train.ali.sub  data/words.out.en.2eps data/graphones.out.en.2eps
		./submodule/LatticeWordSeg/tools/bin/fstprint sentence.fst
		./submodule/LatticeWordSeg/tools/bin/fstprint --isymbols=corpus/tr_babel/train/phones.symbols --osymbols=data/graphones.syms pruned.fst
		#ls -al
babel: themo test_compose hpylmlib.a
		#-mkdir -p data
		cp corpus/tr_babel/train.ref data/train.text
		./scripts/remap_ids.py corpus/tr_babel/words.txt corpus/tr_babel/specials.syms > data/words.syms
		./scripts/remap_ids.py corpus/tr_babel/phones.symbols  corpus/tr_babel/specials.syms > data/phones.syms
		./scripts/int2sym.pl -f 2- corpus/tr_babel/phones.txt  corpus/tr_babel/train.ali | ./scripts/sym2int.pl -f 2- data/phones.syms > data/train.ali
		./scripts/create_grapheme_lexicon.py  --failed-words data/graphemic_lexicon_fails.txt --specials corpus/tr_babel/specials.syms corpus/tr_babel/lexicon.txt  data/grapheme_lexicon.txt data/graphemes.syms
	  ./scripts/create_g2g_fst.py --specials corpus/tr_babel/specials.syms  data/phones.syms  data/graphemes.syms data/graphones.syms  data/g2l.fst.txt
		./scripts/create_oracle_E.py --specials corpus/tr_babel/specials.syms --phones data/phones.syms  --graphemes data/graphemes.syms --graphones data/graphones.syms ./ngram_costs.2.txt.1.12 data/p2g.fst.txt
		./scripts/make_lexicon_fst.pl data/grapheme_lexicon.txt 0.9 '<w>' > data/grapheme_lexicon.fst.txt
		fstcompile --arc_type=log --isymbols=data/graphemes.syms --osymbols=data/words.syms data/grapheme_lexicon.fst.txt | fstarcsort --sort_type=ilabel > data/grapheme_lexicon.fst
		fstcompile --arc_type=log --isymbols=data/graphones.syms --osymbols=data/graphemes.syms data/g2l.fst.txt | fstarcsort --sort_type=olabel > data/g2l.fst
		fstcompile --arc_type=log --isymbols=data/phones.syms    --osymbols=data/graphones.syms data/p2g.fst.txt | fstarcsort --sort_type=olabel > data/p2g.fst
		./themo
		./submodule/LatticeWordSeg/tools/bin/fstprint sentence.fst
		./submodule/LatticeWordSeg/tools/bin/fstprint --isymbols=corpus/tr_babel/train/phones.symbols --osymbols=data/graphones.syms pruned.fst
		#ls -al

clean: 
		rm -f *.a
		rm -f *.o
		rm -f NHPYLM/*.o
		rm -f test_hpylm test_compose
		rm -f *.syms *.fst.txt *.fst

