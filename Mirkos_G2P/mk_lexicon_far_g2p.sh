#export PATH=/home/burget/myworkshop/submodule/LatticeWordSeg/tools/bin:$PATH
export LANG=en_US.UTF-8
outdir=data
mkdir -p $outdir

WORD='<w>'
SENT='</s>'

DataDir=Mirkos_G2P/train_lexicon #/mnt/matylda6/baskar/experiments/G2P/g2p_mirko/g2p_code/g2p_small
phonemeLex=$DataDir/lexicon_librispeech_small.txt

num=`cat $phonemeLex | wc -l`

sel=$((num))

sed -n "17,${num}p" $phonemeLex | awk '{print $1}' | sed -e "s|^ \+||g;s| \+$||g;s|  | |g" > $outdir/all_train.letters
sed -i "s|.| & |g;s|_ \+|_|g;s|__ \+|__|g;s|_ \+_|__|g;s| \+_|_|g;s| \+| |g" $outdir/all_train.letters
sed -n "17,${num}p" $phonemeLex | awk '{$1="";print $0}' | sed -e "s|^ ||g;s|  | |g;s| $||g" > $outdir/all_train.phones

head -n $sel $outdir/all_train.letters > $outdir/train_letters
head -n $sel $outdir/all_train.phones > $outdir/train_prons
tail -100 $outdir/all_train.letters > $outdir/test_letters
tail -100 $outdir/all_train.phones > $outdir/test_prons

train_letters=$outdir/train_letters
train_prons=$outdir/train_prons
test_letters=$outdir/test_letters
test_prons=$outdir/test_prons

GSEP=';'

# Get training lexicon.
cat $train_prons | awk -v sent=$SENT '{ printf "%s",$1; for(i=2;i<=NF;i++) printf " %s",$i; printf " %s\n",sent;}' > ${train_prons}.txt
cat $test_prons | awk -v sent=$SENT '{ printf "%s",$1; for(i=2;i<=NF;i++) printf " %s",$i; printf " %s\n",sent;}' > ${test_prons}.txt

cat $train_letters | awk -v sent=$SENT '{ printf "%s",$1; for(i=2;i<=NF;i++) printf " %s",$i; printf " %s\n",sent;}' > ${train_letters}.txt
cat $test_letters | awk -v sent=$SENT '{ printf "%s",$1; for(i=2;i<=NF;i++) printf " %s",$i; printf " %s\n",sent;}' > ${test_letters}.txt

echo $'<eps> 0\n$ 1' > ${train_prons}.sym
echo $'<eps> 0\n$ 1' > ${train_letters}.sym
cat ${train_prons}.txt | tr ' ' $'\n' | sort -u | grep -v '^$'| awk '{print $1, NR+1}' >> ${train_prons}.sym
cat ${train_letters}.txt | tr ' ' $'\n' | sort -u | grep -v '^$' | awk '{print $1, NR+1}' >> ${train_letters}.sym

# Compile linear transducers.
farcompilestrings --symbols=${train_prons}.sym  --arc_type=log ${train_prons}.txt > ${train_prons}.far
farcompilestrings --symbols=${train_letters}.sym --arc_type=log ${train_letters}.txt > ${train_letters}.far
farcompilestrings --symbols=${train_prons}.sym  --arc_type=log ${test_prons}.txt > ${test_prons}.far
farcompilestrings --symbols=${train_letters}.sym --arc_type=log ${test_letters}.txt > ${test_letters}.far

# Create L2P transducers (P2L = P2G o G2L).
awk '!r {l[nl++]=$1} r {p[np++]=$1} END {
  print "<eps> 0\n<phi> 1\n$'$GSEP'$ 2" > "'$outdir'/train_grphs.sym"
  n=3
  for(j=0; j<np; j++) for(i=0; i<nl; i++)  {
    if((j == 0 && i == 0) || (p[j] != l[i] && (p[j]=="$" || l[i] == "$"))) continue
    print 0, 0, p[j], p[j] "'$GSEP'" l[i]         > "'$outdir'/train_p2g.txt"
    print 0, 0,       p[j] "'$GSEP'" l[i], l[i]   > "'$outdir'/train_g2l.txt"
    if(p[j]=="$" && l[i] == "$") continue;
    print             p[j] "'$GSEP'" l[i], n++ > "'$outdir'/train_grphs.sym"
  }
  print 0 > "'$outdir'/train_p2g.txt"
  print 0 > "'$outdir'/train_g2l.txt"
}' ${train_letters}.sym r=1 ${train_prons}.sym


fstcompile --isymbols=${train_prons}.sym --osymbols=$outdir/train_grphs.sym --arc_type=log $outdir/train_p2g.txt | fstarcsort \
  > $outdir/train_p2g.fst
fstcompile --isymbols=$outdir/train_grphs.sym --osymbols=${train_letters}.sym --arc_type=log $outdir/train_g2l.txt | fstarcsort \
  >  $outdir/train_g2l.fst

# Create epsilon sequence limiting transducer.
awk -v maxeps=1 -F "$GSEP| " '{
  if(NF < 3) next;
  if($1 != "<eps>" && $2 != "<eps>")
    for(i=0; i<= maxeps; i++) print i, 0,   $1 "'$GSEP'" $2
  else
    for(i=0; i< maxeps; i++)  print i, i+1, $1 "'$GSEP'" $2
}
END {
  for(i=0; i<= maxeps; i++)    print i
}' $outdir/train_grphs.sym > "$outdir/train_eps_limiter.txt"


fstcompile --isymbols=$outdir/train_grphs.sym --acceptor --arc_type=log $outdir/train_eps_limiter.txt | fstarcsort \
  > $outdir/train_eps_limiter.fst

# Create edit distance transducer for L.
awk '
{l[nl++]=$1}
END {
  for(j=0; j<nl; j++) for(i=0; i<nl; i++)  {
    if((j == 0 && i == 0) || (l[j] != l[i] && (l[j]=="$" || l[i] == "$"))) continue
    print 0, 0, l[j], l[i], l[j]!=l[i] > "'$outdir'/letter_edit.txt"
  }
  print 0 > "'$outdir'/letter_edit.txt"
}' ${train_letters}.sym

fstcompile --isymbols=${train_letters}.sym --osymbols=${train_letters}.sym --arc_type=log $outdir/letter_edit.txt | fstarcsort \
  > $outdir/letter_edit.fst

# Create edit distance transducer for P.
awk '
{l[nl++]=$1}
END {
  for(j=0; j<nl; j++) for(i=0; i<nl; i++)  {
    if((j == 0 && i == 0) || (l[j] != l[i] && (l[j]=="$" || l[i] == "$"))) continue
    print 0, 0, l[j], l[i], l[j]!=l[i] > "'$outdir'/phone_edit.txt"
  }
  print 0 > "'$outdir'/phone_edit.txt"
}' ${train_prons}.sym

fstcompile --isymbols=${train_prons}.sym --osymbols=${train_prons}.sym --arc_type=log $outdir/phone_edit.txt | fstarcsort \
  > $outdir/phone_edit.fst