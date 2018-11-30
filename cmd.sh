# "queue.pl" uses qsub. The options to it are options to qsub. 

### DEFAULT CONFIGURATION:
# Run with qsub (the reources are typically site-dependent),
export train_cmd="queue.pl -l ram_free=1.5G,mem_free=1.5G"
export plain_cmd="$train_cmd"
export decode_cmd="queue.pl -l ram_free=2.5G,mem_free=2.5G"
export cuda_cmd="queue.pl -l gpu=1"


# Run locally,
#export plain_cmd=run.pl
#export train_cmd=run.pl
#export decode_cmd=run.pl
#export cuda_cmd=run.pl

### Other tools
#cn_gpu=/data/sls/scratch/yzhang87/code/master/cntk/bin/cntk 

### SITE-SPECIFIC CONFIGURATION (OVERRIDES DEFAULT):
case "$(hostname -d)" in
  "fit.vutbr.cz") # BUT cluster,
    declare -A user2matylda=([iveselyk]=matylda5 [karafiat]=matylda3 [ihannema]=matylda5 [baskar]=matylda6)
    matylda=${user2matylda[$USER]}
    #queue="all.q@@speech"
    queue="long.q@@blade*"

   gpu_queue="long.q@supergpu*,long.q@pcspeech-gpu,long.q@pcgpu*,long.q@dellgpu*,long.q@pcgpu*"
   #queue="long.q@supergpu*,long.q@pcspeech-gpu,long.q@pcgpu*,long.q@dellgpu*,long.q@pcgpu*"

    #queue="all.q@@blade*"
   #gpu_queue="long.q@supergpu*"
   # gpu_queue="long.q@supergpu4*"
    #gpu_queue="long.q@@pco203*,long.q@supergpu*,long.q@dellgpu*,long.q@pcspeech-gpu,long.q@pcgpu*"
    #gpu_queue="long.q@@gpu,long.q@@speech-gpu,long.q@supergpu*,long.q@dellgpu*,long.q@pcspeech-gpu,long.q@pcgpu*"
     #,long.q@@babel-eval*,long.q@supergpu*,long.q@dellgpu*,long.q@pcspeech-gpu,long.q@pcgpu*"
    export plain_cmd="run.pl" # Runs locally (initial GMM training),
    #export train_cmd="queue.pl -q $queue -l ram_free=1.5G,mem_free=1.5G,${matylda}=0.25"
    export train_cmd="queue.pl -q $queue -pe smp 5 -l ram_free=1.5G,mem_free=1.5G"
    export train_cmd="run.pl -q -pe smp 5 -l ram_free=1.5G,mem_free=1.5G"
    export decode_cmd="queue.pl -q $queue -l ram_free=2.5G,mem_free=2.5G,${matylda}=0.1"
    export cuda_cmd="queue.pl -q $gpu_queue -l gpu=1 -l ram_free=25G,mem_free=25G"
    #export cuda_cmd="queue.pl -q $gpu_queue -l gpu=1"
    
    #cn_gpu=/mnt/matylda3/karafiat/BABEL/GIT/cntk.linux-gcc/bin.2015-11-30/cntk
    #cn_gpu=/mnt/matylda3/karafiat/BABEL/GIT/cntk/bin.2015-11-21.483ee2d5e84a7aaa3fa34368664d1c5118570255/cntk
    #cn_gpu=/mnt/matylda3/karafiat/BABEL/GIT/cntk/build.gpu.2015-11-18.bf2149507cc39ca5708d3815678d93402421dde4/bin/cntk
    #cn_gpu=/mnt/matylda3/karafiat/BABEL/GIT/cntk/build.gpu.2016-1-7.a93f8f46949877c7a6b7f58e35f8105856251f90/bin/cntk
    #cn_gpu=/mnt/matylda3/karafiat/BABEL/GIT/cntk/build.gpu.2016-01-25.myBranchForMPI.incKaldi/bin/cntk
    #cn_gpu=/mnt/matylda3/karafiat/BABEL/GIT/cntk.github/build.gpu.2016-02-29.e656377d57953dd26605c35decfb074576b5f972.MyBranch/bin/cntk
    #cn_gpu=/mnt/matylda3/karafiat/BABEL/GIT/cntk/build.gpu.2016-03-05.7de2a63ec70726dd176e6fff51a97c1e17051a9b.myBranch/bin/cntk
    #cn_gpu=/mnt/matylda3/karafiat/BABEL/GIT/cntk/build.gpu.latest
    #cn_gpu=/mnt/matylda3/karafiat/BABEL/GIT/cntk.YuZhang/build.gpu.2016-07-20.6a0a4c9eff296a1451731a57214967400a6522f2.MyBranch.cuda8.0/bin/cntk
   # cn_gpu=/mnt/matylda3/karafiat/BABEL/GIT/cntk.YuZhang/build.gpu.2016-07-22.4be1af959ff2cf72ec3c05974faa2a7a8b4b617b.MyBranch.cuda8.0/bin/cntk
    cn_gpu=/mnt/matylda3/karafiat/BABEL/GIT/cntk.YuZhang/build.gpu.latest.testarch/bin/cntk
   #cn_gpu=/mnt/matylda6/baskar/CNTK_v2/bin/cntk
    #cn_gpu=/mnt/matylda6/baskar/CNTK_Martin/bin/cntk
    #cn_gpu=/mnt/matylda6/baskar/CNTK_aug16/CNTK-1.1/bin/cntk
    #cn_gpu=/mnt/matylda6/baskar/CNTK-20160126-Linux-64bit-ACML5.3.1-CUDA7.0/bin/cntk
    #cn_gpu=/homes/kazi/baskar/CNTK-20160126-Linux-64bit-ACML5.3.1-CUDA7.0/CNTK-20160126-Linux-64bit-ACML5.3.1-CUDA7.0/bin/cntk
    cn_cpu=$cn_gpu
  ;;
esac

