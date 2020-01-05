#!/bin/bash


export PATH=$PATH:/home/xgzhu/apps/collafl-dyninst
export AFL_NO_UI=1 
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/xgzhu/apps/collafl-dyninst
# $0: runfuzz.sh itself; $1: path to output directory
# $2: fuzzing seed dir;
# $3: path to target binary;  ${@:4}: parameters running targets
# bash runfuzz.sh ../outputs/becread1 ../target-bins/untracer_bins/binutils/readelf ../target-bins/untracer_bins/binutils/seed_dir/ -a @@

OUTDIR=${1}_collafl
SEEDS=$2
TARGET=$3
VERSION=$4
FUZZTIME=$5
WITHDICT=$6
TIMEOUT=$7
PARAMS=`echo ${@:8}`



NAME=`echo ${TARGET##*/}`
INSTNAME=${NAME}_inst


# mkdir $OUTDIR
# ./CollAFLDyninst${VERSION} -i $TARGET  -o  ${OUTDIR}/${INSTNAME}
# sleep 1

if [ "$WITHDICT"x = "nodict"x ]
then
    COMMD="./collafl${VERSION} -i $SEEDS -o ${OUTDIR}/out -t $TIMEOUT -m 1G -- ${OUTDIR}/${INSTNAME} $PARAMS"
else
    COMMD="./collafl${VERSION} -i $SEEDS -o ${OUTDIR}/out -x ${WITHDICT} -t $TIMEOUT -m 1G -- ${OUTDIR}/${INSTNAME} $PARAMS"
fi


(
    ${COMMD}
)&
sleep $FUZZTIME
# ctrl-c
ps -ef | grep "$COMMD" | grep -v 'grep' | awk '{print $2}' | xargs kill -2

rm ${OUTDIR}/${INSTNAME}
# chmod 777 -R $OUTDIR
sleep 1

