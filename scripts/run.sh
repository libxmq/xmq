#!/bin/sh

if [ -n "$DEBUG" ]
then
    set -x
fi

WORKING_DIR="$(pwd)"

cd "$(dirname "$0")" || exit 1
jarfile=$(pwd)/$(basename "$0")
jarfolder="$( (cd "$(dirname "$jarfile")" && pwd -P) )"

cd "$WORKING_DIR" || exit 1

if [ -n "$(which java)" ]
then
    javaexe="$(which java)"
elif [ -n "$JAVA_HOME" ] && [ -x "$JAVA_HOME/bin/java" ]
then
    javaexe="$JAVA_HOME/bin/java"
elif type -p java > /dev/null 2>&1
then
    javaexe=$(type -p java)
elif [ -x "/usr/bin/java" ]
then
    javaexe="/usr/bin/java"
else
    echo "Unable to find Java"
    exit 1
fi

"$javaexe" -Dsun.misc.URLClassPath.disableJarChecking=true -ea $JAVA_OPTS --enable-preview -jar "$jarfile" $RUN_ARGS "$@"
RC="$?"

if [ "$RC" != "0" ]
then
    if [ -n "$DEBUG" ]
    then
        echo Failure when executing command:
        echo "$javaexe" -Dsun.misc.URLClassPath.disableJarChecking=true -ea $JAVA_OPTS --enable-preview -jar "$jarfile" $RUN_ARGS "$@"
    fi
fi

exit $RC
