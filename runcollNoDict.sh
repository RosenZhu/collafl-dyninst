#!/bin/bash


export PATH=$PATH:/apps/collafl-dyninst
export AFL_NO_UI=1 

# $0: runfuzz.sh itself; $1: path to output directory
# $2: fuzzing seed dir;
# $3: path to target binary;  ${@:4}: parameters running targets
# bash runfuzz.sh ../outputs/becread1 ../target-bins/untracer_bins/binutils/readelf ../target-bins/untracer_bins/binutils/seed_dir/ -a @@


OUTDIR=${1}_collafl
SEEDS=$2
TARGET=$3
VERSION=$4
PARAMS=`echo ${@:5}`

NAME=`echo ${TARGET##*/}`
INSTNAME=${NAME}_inst


mkdir $OUTDIR
./CollAFLDyninst${VERSION} -i $TARGET  -o  ${OUTDIR}/${INSTNAME}

COMMD="./collafl${VERSION} -i $SEEDS -o ${OUTDIR}/out -t 500 -m 1G -- ${OUTDIR}/$INSTNAME $PARAMS"

(
    ${COMMD}
)&
sleep 1m
# ctrl-c
ps -ef | grep "$COMMD" | grep -v 'grep' | awk '{print $2}' | xargs kill -2

chmod 777 -R $OUTDIR
sleep 1

