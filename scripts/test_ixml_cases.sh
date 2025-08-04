#!/bin/bash

for i in $@
do
    echo $i
    if ! ./xmq --ixml=$i
    then
        exit 1
    fi
done
