#!/bin/bash
IN=$1
OUT=$2
MAXT=$3

test() {
    FILE=$1
    if [[ ! -f tecnicofs ]]
    then
        make
    fi

    if [[ -f $FILE ]]
    then
        for ((i = 1; i <= $2; i++))
        do
            FILE=${FILE##*/}
            printf "InputFile = %s NumThreads = %d\n" $FILE $i
            ./tecnicofs $IN/$FILE $OUT/$FILE-$i.txt $i |& grep TecnicoFS
        done
    fi
}

if [[ $# -ne 3 ]]
then
    echo "Error: Invalid input" >> /dev/stderr
    echo "Input should be: /runTests.sh inputdir outputdir maxthreads" >> /dev/stderr
    exit 1
fi

if [[ ! -d $IN ]]
then
    echo "Error: Input folder does not exist"  >> /dev/stderr
    exit 1
fi

if [[ $OUT == "." ]]
then
    OUT=$(pwd)
fi

if [[ ! -d $OUT ]]
then
    mkdir -p $OUT
fi

if [[ $MAXT -gt 0 ]]
then
    for FILE in $IN/*
    do
        test $FILE $MAXT
    done
else
    echo "Error: Invalid number of threads" >> /dev/stderr
fi
