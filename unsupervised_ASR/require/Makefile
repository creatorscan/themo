SHELL := /bin/bash
#CXXFLAGS := -g3 -O3  -fpermissive -I./NHPYLM/ext_deps/ -I./NHPYLM/ -I./submodule/LatticeWordSeg/tools/include/ $(CXXFLAGS) 
openfst=openfst
boost=../boost_1_61_0
nhpylm=require/NHPYLM
kaldi=/mnt/matylda6/baskar/Kaldi/kaldi/src
require=/mnt/matylda6/baskar/experiments/G2P/24thFeb18/require
CXXFLAGS :=  -ggdb -fpermissive -I./$(openfst)/src/include -I$(require) -std=c++14 $(CXXFLAGS)

#CXX = clang++
CXX := g++-6.4
CC := gcc-6.4

.PHONY: clean test wsj babel

beamtrim: beamtrim.o
	$(CXX) $(CXXFLAGS) -L$(PWD)/$(openfst)/src/lib/.libs -Wl,-rpath=$(PWD)/$(openfst)/src/lib/.libs -ldl -lfst $^ -o $@


clean: 
		rm -f *.a
		rm -f *.o
		rm -f NHPYLM/*.o
		rm -f test_hpylm test_compose
		rm -f *.syms *.fst.txt *.fst

