SHELL := /bin/bash
FSTDIR=/mnt/matylda6/baskar/experiments/G2P/openfst-1.5.4
CXXFLAGS :=  -ggdb -fpermissive -I./NHPYLM/ext_deps/ -I./NHPYLM/ -I$(FSTDIR)/src/include -I../../boost_1_61_0 -std=c++14 -ldl -lpthread$(CXXFLAGS)

CXX = g++-6.4
#CXX = clang++
#CXX := g++-6.1
CC := $(CXX)
#CC := gcc-6.1

.PHONY: clean test

all: hpylmlib.a test_hpylm test_compose

hpylmlib.a: NHPYLM/Dictionary.o NHPYLM/NHPYLM.o NHPYLM/HPYLM.o NHPYLM/Restaurant.o
	ar cru $@ $?

test_hpylm: test_hpylm.o hpylmlib.a

test_compose: test_compose.o NHPYLMFst.o transducer_E.o hpylmlib.a
	$(CXX) $(CXXFLAGS)  -L$(PWD)/submodule/LatticeWordSeg/tools/lib -Wl,-rpath=$(PWD)/submodule/LatticeWordSeg/tools/lib -lfst $^ -o $@

training_g2p: training_g2p.o HPYLMFst.o hpylmlib.a
	$(CXX) $(CXXFLAGS)  -L$(FSTDIR)/lib -L$(PWD)/../boost_1_61_0/stage/lib -Wl,-rpath=$(FSTDIR)/lib,-rpath=$(PWD)/../boost_1_61_0/stage/lib -lfst -lfstfar $^ -o $@

# -lboost_program_options

train: train.o HPYLMFst.o hpylmlib.a
	$(CXX) $(CXXFLAGS)  -L$(FSTDIR)/lib -L$(PWD)/../boost_1_61_0/stage/lib -Wl,-rpath=$(FSTDIR)/lib,-rpath=$(PWD)/../boost_1_61_0/stage/lib -lfst -lfstfar $^ -o $@
#
#	$(CXX) $(CXXFLAGS) -L$(PWD)/submodule/LatticeWordSeg/tools/lib -L$(PWD)/submodule/LatticeWordSeg/tools/lib/fst -Wl,-rpath=$(PWD)/submodule/LatticeWordSeg/tools/lib,-rpath=$(PWD)/submodule/LatticeWordSeg/tools/lib/fst -lfst -lfstfar $^ -o $@
testing_g2p: testing_g2p.o
	$(CXX) $(CXXFLAGS)  -L$(FSTDIR)/lib -L$(PWD)/../../boost_1_61_0/stage/lib -Wl,-rpath=$(FSTDIR)/lib,-rpath=$(PWD)/../../boost_1_61_0/stage/lib -lfst -lfstfar $^ -o $@

#	$(CXX) $(CXXFLAGS) -L$(PWD)/submodule/LatticeWordSeg/tools/lib -L$(PWD)/submodule/LatticeWordSeg/tools/lib/fst -Wl,-rpath=$(PWD)/submodule/LatticeWordSeg/tools/lib,-rpath=$(PWD)/submodule/LatticeWordSeg/tools/lib/fst -lfst -lfstfar $^ -o $@
talign_g2p: talign_g2p.o
	$(CXX) $(CXXFLAGS)  -L$(FSTDIR)/lib -L$(PWD)/../boost_1_61_0/stage/lib -Wl,-rpath=$(FSTDIR)/lib,-rpath=$(PWD)/../boost_1_61_0/stage/lib -lfst -lfstfar $^ -o $@

HPYLMFst_demo: HPYLMFst_demo.o HPYLMFst.o hpylmlib.a
	$(CXX) $(CXXFLAGS)  -L$(PWD)/submodule/LatticeWordSeg/tools/lib -L$(PWD)/submodule/LatticeWordSeg/tools/lib/fst -Wl,-rpath=$(PWD)/submodule/LatticeWordSeg/tools/lib,-rpath=$(PWD)/submodule/LatticeWordSeg/tools/lib/fst -lfst -lfstfar $^ -o $@

test: test_compose
	  ./create_oracle_E.py ./ngram_costs.txt.1.9 prde.fst.txt phones.syms graphemes.syms graphones.syms
		./submodule/LatticeWordSeg/tools/bin/fstcompile prde.fst.txt > prde.fst
		./test_compose 
		#./submodule/LatticeWordSeg/tools/bin/fstprint sentence.fst
		#ls -al

clean: 
		rm -f *.a
		rm -f *.o
		rm -f NHPYLM/*.o
		rm -f test_hpylm test_compose
		rm -f *.syms *.fst.txt *.fst

