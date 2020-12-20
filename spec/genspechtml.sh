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
    <b>text</b> consists of all valid utf8 exluding
    these 6 reserved characters <b>= ' ( ) { }</b>
    as well as whitespace (SPACE,TAB,LF,CR) and the <b>nul</b> character.
    (Examples of text are: 123.4 $U\P$ http://www.zzz.yyy/index.html /file/help.txt C:\adir)
    </p>

    <p>
    <b>key</b> is <b>text</b> but with the additional xml 1.0 tag name restrictions.
    (Examples of keys are: name age speed INFO order)
    </p>

    <p>
    <b>quoted content</b> starts with one, three or more single quotes <b>'</b> and ends with an equal
     amount of singel quotes. Use n+1 quotes to quote an utf8 string containing at most n consecutive quotes.
     Two single quotes are always the empty string.
     (Examples of quoted contents are: '' 'John Doe' '5+(8*9)' '''(&gt;'&lt;)''' )
    </p>

    <p>
    Text that starts with <b>//</b> or <b>/*</b> is a comment, and will end with <b>eol</b> or <b>*/</b>
    Quote such text to prevent it from being a comment.
    </p>

    <p>
    A <b>key</b> can be standalone, or followed by <b>=</b> or <b>{...}</b>.
    </p>

    <p>
    A <b>key</b> kan be followed by parentheses <b>(...)</b> within which there are attributes.
    (Examples of attributes are: div(id=123) )
    </p>

    <p>
    <b>whitespace</b> is a separator that is irrelevant except:
       <ol>
          <li>when separating <b>text</b> from the next <b>key</b>.</li>
          <li>when separating <b>quoted content</b> from the following standalone <b>quoted content</b>.</li>
          <li>inside <b>quoted content</b></li>
       </ol>
    </p>

    <p>
       However, inside <b>quoted content</b>:
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
