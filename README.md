## Grapheme to Phoneme Converter

This repo contains simple G2P converter in **Mirkos_G2P**. This code is based on the work published in [Bayesian G2P](http://www.fit.vutbr.cz/research/groups/speech/publi/2017/hannemann_icassp2017_0002836.pdf)

## Running example code
1. `data_eko` contains sample files of letter and phoneme generated using the `mk_lexicon_far_eko_g2p.sh`
2. `mk_lexicon_far_eko_g2p.sh` generates sample inputs in a desired format by feeding on input training dictionary
3. `training_g2p` allows training a mapping between input graphemes and phonemes
4. `testing_g2p` validates the training G2P model using sample test letters
5. Executing `bash test.sh` traverses from steps 2 to 4 to provide desired phoneme sequences for test graphemes.
