#!/bin/bash

PROG=$1

if [ ! -x "$PROG" ]
then
    echo First argument must be the executable binary to use/test!
    exit 1
fi

TESTDIR=test_output

rm -rf $TESTDIR
mkdir -p $TESTDIR

OUT=xmq_spec.html

function testcase () {
    local NAME=${1%.xmq}
    local GENXML=$TESTDIR/gen_$NAME.xml
    local GENXMQ=$TESTDIR/gen_$NAME.xmq

    local DIR=BOTH
    if [ "$3" != "" ]
    then
        DIR="$3"
    fi

    local OPTIONS=$4

    RC=true

    if [ "$DIR" = "BOTH" ] || [ "$DIR" = "RIGHT" ]
    then
        # From xmq to xml
        $PROG --nodec spec/$1 > $GENXML

        diff -q spec/$2 $GENXML
        if [ $? != "0" ]
        then
            RC=false
            echo ERROR $NAME ===================================
            diff spec/$2 $GENXML
            echo ERROR $NAME ===================================
            echo Failed test! Generated xml not as expected!
            exit 1
        fi
    fi

    if [ "$DIR" = "BOTH" ] || [ "$DIR" = "LEFT" ]
    then
        # From xml back to xmq
        $PROG $OPTIONS spec/$2 > $GENXMQ

        diff -q spec/$1 $GENXMQ
        if [ $? != "0" ]
        then
            RC=false
            echo ERROR $NAME ===================================
            diff spec/$1 $GENXMQ
            echo ERROR $NAME ===================================
            echo Failed test! Regenerated xmq not as expected!
            exit 1
        fi
    fi

    if [ "$RC" = "true" ]
    then
        echo OK $NAME
    fi
}

cat > xmq_spec.css <<EOF
html { box-sizing: border-box; }
*, *:before, *:after { box-sizing: inherit; }
.info {
  font-style: italic;
  width: 80%;
}
.row {
  display: flex;
  align-items: stretch;
  margin:0;
  padding:0;
}
.arrow {
  display: inline-block;
}
.box {
  width:40%;
  padding:10px;
  border: solid 1px black;
  align-self: center;
  display: inline-block;
  overflow: hidden;
}
.spacing {
  padding:10px;
  align-self: center;
  display: inline-block;
}
EOF

TODAY=$(date +%Y-%m-%d)

cat > $OUT <<EOF
<!DOCTYPE html>
<html>
<header>
<link type="text/css" href="xmq_spec.css" rel="stylesheet"/>
<body>

    <h1>XMQ specification</h1>

    As of ${TODAY} and in flux. By Fredrik Öhrström oehrstroem@gmail.com

    <p>
    <b>safe text</b> consists of all valid utf8 exluding
    these 6 characters <b>= ' ( ) { }</b>
    and whitespace and the nul character.
    </p>

    <p>
    <b>name</b> is an xml tag, ie it is safe text with the
    additional xml 1.0 tag name restrictions.
    (I.e. start with letter or underscore etc.)
    </p>

    <p>
    <b>unquoted content</b> consists of safe text.
    </p>

    <p>
    <b>quoted content</b> can contain all valid utf8 except the nul character.
    </p>

    <p>
    <b>whitespace</b> is a separator that is irrelevant except:
       <ol>
          <li>when separating unquoted content from the next name.</li>
          <li>when separating quoted content from quoted content.</li>
          <li>inside quoted content</li>
       </ol>
    </p>

    <p>
       However, inside quoted content:
       <ol>
       <li>A leading or ending sequence of Whitespace_NewLine_Whitespace will always be trimmed.</li>
       <li>Incidental whitespace will always be trimmed, when there is at least one newline.</li>
       </ol>
    </p>

    <p>A newline is implicitly inserted between two standalone quoted contents.</p>

    <p>To avoid cluttering the examples below with xml/html5 declarations, --nodec is used.</p>
EOF

RIGHT=""
LEFT=""

while IFS= read -r line
do
    if [[ $line == \#* ]]
    then
        line=$(echo "$line" | cut -c 2- )
        echo "<div class=\"info\">$line</div>" >> $OUT
    elif [[ $line == BOTH* ]]
    then
        FROM=$(echo "$line" | cut -f 2 -d ' ')
        TO=$(echo "$line" | cut -f 3 -d ' ')
        OPTIONS=$(echo "$line" | cut -f 4- -d ' ')
        LEFT=$TO
        RIGHT=$TO
        echo "<div class=\"row\">" >> $OUT

        echo "<pre class=\"box\">" >> $OUT
        $PROG --color --nodec -v --output=html spec/$FROM >> $OUT
        echo -n "</pre>" >> $OUT

        echo -n "<pre class=\"spacing\">⟷</pre>" >> $OUT

        echo "<pre class=\"box\">" >> $OUT
        cat spec/$TO | sed 's/&/\&amp;/g' | sed 's/</\&lt;/g' | sed 's/>/\&gt;/g' >> $OUT
        echo -n "</pre>" >> $OUT

        echo "</div>" >> $OUT

        testcase $FROM $TO BOTH $OPTIONS

    elif [[ $line == RIGHT* ]]
    then
        FROM=$(echo "$line" | cut -f 2 -d ' ')
        TO=$(echo "$line" | cut -f 3 -d ' ')
        OPTIONS=$(echo "$line" | cut -f 3- -d ' ')

        echo "<div class=\"row\">" >> $OUT

        echo "<pre class=\"box\">" >> $OUT
        cat spec/$FROM | sed 's/&/\&amp;/g' | sed 's/</\&lt;/g' | sed 's/>/\&gt;/g' >> $OUT
        echo -n "</pre>" >> $OUT

        echo -n "<pre class=\"spacing\">↗</pre>" >> $OUT

        if [ "$TO" != "$RIGHT" ]
        then
            echo "<pre class=\"box\">" >> $OUT
            cat spec/$TO | sed 's/&/\&amp;/g' | sed 's/</\&lt;/g' | sed 's/>/\&gt;/g' >> $OUT
            echo -n "</pre>" >> $OUT
        fi

        echo "</div>" >> $OUT

        testcase $FROM $TO RIGHT $OPTIONS
    fi
done < "spec/list.txt"

#    testname=$(basename ${i%.xml})
    # From xml to xmq
#    echo "<b>${testname}</b> " >> $OUT
#    cat spec/${testname}.info >> $OUT
#    echo "<br/>" >> $OUT
#    echo "<div class=\"row\">" >> $OUT
#    echo "<pre class=\"box\">" >> $OUT
#    cat $i | sed 's/&/\&amp;/g' | sed 's/</\&lt;/g' | sed 's/>/\&gt;/g' >> $OUT
#    echo -n "</pre><pre class=\"spacing\">⟷</pre>" >> $OUT
#    echo "<pre class=\"box\">" >> $OUT
#    cat spec/${testname}.xmq >> $OUT
#    echo "</pre>" >> $OUT
#    echo "</div>" >> $OUT
#    echo "<br/>" >> $OUT
#done

cat >> $OUT <<EOF
</body>
</html>
EOF
