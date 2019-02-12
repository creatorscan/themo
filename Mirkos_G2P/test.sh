main=/mnt/matylda6/baskar/experiments/G2P/g2p_mirko/g2p_code

./testing_g2p data_pashto/tobe_decoded.letters.far data_pashto/tobe_decoded.letters.far data_pashto/train_p2g.fst data_pashto/train_g2l.fst data_pashto/train_eps_limiter.fst data_pashto/letter_edit.fst data_pashto/phone_edit.fst data_pashto/hpylm.iteration1p2g.fst &> $main/data_pashto/tobe_decoded.letters.prons.decoded

cat $main/data_pashto/tobe_decoded.letters.prons.decoded | grep -v "^$" | grep -v "ID" | sed "s|:||g;s|^ \+||g;s|  \+| |g"  | utils/int2sym.pl -f 1 > $main/data_pashto/tobe_decoded.letters.prons.decoded.sym

awk '{print $1}' /mnt/matylda6/baskar/experiments/kaldi/Babel/exp_y4/l2p/lang_prepare/LANGUAGES/bp104-Pashto-Base.v0_NWUdct/data/local/dict/tmp/gen_dic > $main/data_pashto/tobedec.words

paste $main/data_pashto/tobedec.words $main/data_pashto/tobe_decoded.letters.prons.decoded.sym > $main/data_pashto/tobe_decode.final.dic
