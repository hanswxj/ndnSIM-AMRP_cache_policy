export CACHE_SIZE=100
export CACHE_POLICY=nfd::cs::lru
export FREQUENCY=100
export NumberOfContents=1000
export ZipfParam=0.8

# cache=(nfd::cs::lirs nfd::cs::dlirs)
cache=(nfd::cs::lru nfd::cs::lrfu nfd::cs::ccp nfd::cs::lirs nfd::cs::dlirs)
cache_size=(20 50 100 150 200)
zipf=(0.6 0.7 0.8 0.9 1.0)
frequency=(100 200 300 400 500)

cd /home/wxj/ndnSIM-2.6/ns-3
mkdir -p log/zipf_0.8/policy_vs_cache_size
for size in ${cache_size[@]}
do
    mkdir -p log/zipf_0.8/policy_vs_cache_size/$size
    for policy in ${cache[@]}
    do
        CACHE_POLICY=$policy
        CACHE_SIZE=$size        
        NS_LOG=nfd.ContentStore ./waf --run=3x3-ndn-lirs-grid &> log/zipf_0.8/policy_vs_cache_size/$size/$CACHE_POLICY-$CACHE_SIZE-$ZipfParam.log
        echo "Zipf_α-"$ZipfParam":  finish policy="$CACHE_POLICY "vs cache_size="$CACHE_SIZE
    done
done

# mkdir -p log/policy_vs_zipf
# for alpha in ${zipf[@]}
# do
#     mkdir -p log/policy_vs_zipf/α=$alpha
#     for policy in ${cache[@]}
#     do
#         CACHE_POLICY=$policy
#         ZipfParam=$alpha
#         NS_LOG=nfd.ContentStore ./waf --run=3x3-ndn-lirs-grid &> log/policy_vs_zipf/α=$alpha/$CACHE_POLICY-$CACHE_SIZE-$ZipfParam.log
#         echo "Size-"$CACHE_SIZE":  finish policy="$CACHE_POLICY "vs zipf_α="$ZipfParam
#     done
# done

mkdir -p log/policy_vs_frequency
for fre in ${frequency[@]}
do
    mkdir -p log/policy_vs_frequency/$fre
    for policy in ${cache[@]}
    do
        CACHE_SIZE=100
        CACHE_POLICY=$policy
        ZipfParam=0.7
        FREQUENCY=$fre
        NS_LOG=nfd.ContentStore ./waf --run=3x3-ndn-lirs-grid &> log/policy_vs_frequency/$fre/$CACHE_POLICY-$FREQUENCY-$CACHE_SIZE-$ZipfParam.log
        echo "Frequency-"$FREQUENCY": Size-"$CACHE_SIZE":  finish policy="$CACHE_POLICY "vs zipf_α="$ZipfParam
    done
done