#!/bin/sh

cat <<EOF > yaep_full_source.c
/****************** YAEP parser single source file headers **********************/
#ifndef BUILDING_XMQ

#include  "yaep.h"
#include<assert.h>
#include<setjmp.h>

#endif

#ifdef YAEP_MODULE
EOF

cat src/allocate.h src/hashtab.h src/objstack.h src/vlobject.h >> yaep_full_source.c
echo "/****************** YAEP parser single source file code **********************/" >> yaep_full_source.c
cat src/allocate.c src/hashtab.c src/objstack.c src/vlobject.c src/yaep.c  >> yaep_full_source.c
echo "/****************** YAEP parser single source file end **********************/" >> yaep_full_source.c

cat <<EOF >> yaep_full_source.c
#endif // YAEP_MODULE
EOF

cat yaep_full_source.c | \
    sed 's/__cplusplus/NOT_DEFINED/g' | \
    sed 's/^extern //g' | \
    sed 's/^#include "/\/\/include "/g' | \
    sed 's/^#include"/\/\/include"/g' | \
    sed 's/LOAD_YAEP/#include\"parts\/yaep.h\"/g' | \
    sed 's/LOAD_SETJMP/#include<setjmp.h>/g' \
    > tt.c

mv tt.c yaep_full_source.c

cat src/yaep.h  |  \
    sed 's/__cplusplus/NOT_DEFINED/g' | \
    sed 's/extern //g' | \
    sed 's/__YAEP__/YAEP_H/g' |  \
    sed 's|^.endif.*YAEP_H.*|#define YAEP_MODULE\n#endif // YAEP_H|g'  \
    > yaep_full_source.h

cp yaep_full_source.h ../parts/yaep.h
cp yaep_full_source.c ../parts/yaep.c

rm yaep_full_source.h yaep_full_source.c
