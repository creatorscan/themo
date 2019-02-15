#!/bin/bash

# specify your main directory here
main=Mirkos_G2P
data=$main/data

bash -xe $main/mk_lexicon_far_g2p.sh

### fetch eos_id  # bad hack
eos_id=$(grep "</s>;</s>" $data/train_grphs.sym | awk '{print $2}')

### training ###
./training_g2p $data/train_prons.far $data/train_letters.far $data/train_p2g.fst \
    $data/train_g2l.fst $data/train_eps_limiter.fst $eos_id ${data}_out

### testing ###
./testing_g2p $data/test_letters.far $data/test_letters.far $data/train_p2g.fst \
    $data/train_g2l.fst $data/train_eps_limiter.fst $data/letter_edit.fst \
    $data/phone_edit.fst ${data}_out/hpylm.iteration3.fst &> $data/test_prons_raw.decoded

grep "sub " $data/test_prons_raw.decoded | cut -f 1 -d "("  | sed "s|^ \+||g" | \
    utils/int2sym.pl -f 1- $data/train_prons.sym  > $data/test_prons.decoded 

awk '{print $1}' $data/test_letters.txt > $data/tmp

paste $data/tmp $data/test_prons.decoded > $data/test_final.dic

rm $data/tmp
