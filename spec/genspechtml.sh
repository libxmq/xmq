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
    local OPTIONSS=$5

    RC=true

    if [ "$DIR" = "BOTH" ] || [ "$DIR" = "RIGHT" ]
    then
        # From xmq to xml
        $PROG $OPTIONSS --nodec spec/$1 > $GENXML

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
        $PROG $OPTIONS $OPTIONSS spec/$2 > $GENXMQ

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
    <b>TEXT</b> consists of all valid utf8 exluding
    these 6 reserved characters <b>= ' ( ) { }</b>
    as well as whitespace (SPACE,TAB,LF,CR) and the <b>nul</b> character.
    (Examples of text are: 123.4 $U\P$ http://www.zzz.yyy/index.html /file/help.txt C:\adir)
    </p>

    <p>
    <b>TEXT tag</b> is TEXT but with the additional xml tag name restrictions.
    (Examples of tags are: name age speed INFO order)
    </p>

    <p>
    <b>TEXT attribute</b> is TEXT but with the additional xml attribute name restrictions.
    </p>

    <p>
    <b>QUOTE</b> starts with one, three or more single quotes <b>'</b> and ends with an equal
     amount of singel quotes. Use n+1 quotes to quote an utf8 string containing at most n consecutive quotes.
     Two single quotes are always the empty string.
     (Examples of quoted contents are: '' 'John Doe' '5+(8*9)' '''(&gt;'&lt;)''' )
    </p>

    <p>
     <b>xyz</b> is the self-closing node <b>&lt;xyz/&gt;</b>
    </p>

    <p>
     <b>xyz { ... }</b> is a node with children <b>&lt;tag&gt; ... &lt;/tag&gt;</b>
    </p>

    <p>
     <b>'utf8'</b> is standalone quoted content, inserted as is into the xml with &amp;&lt; et.al. protected.
    </p>

    <p>
    <b>tag = 'utf8'</b> is syntactic sugar for <b>tag { 'utf8' }</b> which is <b>&lt;tag&gt;utf8&lt;/tag&gt;</b>
    </p>

    <p>
     <b>tag = text</b> is syntactic sugar for <b>tag { 'text' }</b> which is <b>&lt;tag&gt;text&lt;/tag&gt;</b>
    </p>

    <p>
     <b>tag(id=123 class='x y') { ... }</b> is  <b>&lt;tag id="123" class="x y" &gt; ... &lt;/tag&gt;</b>
    </p>

    <p>
    Text that starts with <b>//</b> or <b>/*</b> is a comment, and will end with <b>eol</b> or <b>*/</b>.
    Quote such text to prevent it from being a comment. Comments are not permitted just
    before an <b>=</b> or a <b>{</b>, nor are they permitted inside parentheses.
    </p>

    <p>
    XMQ permits multiple root nodes in a single XMQ file if you have supplied the --root=xyz option.
    If the xmq file already has a root node xyz, then nothing happens. If not, then an xyz root node is added
    wrapping the multiple root nodes in the xmq file.
    </p>

    <p>
    <b>whitespace</b> is a separator that is irrelevant except:
       <ol>
          <li>when separating <b>text</b> from the next <b>tag</b>.</li>
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

    <p>
    Three reasons for possible differences in the XML when converting XML->XMQ->XML.
    <ol>
    <li>CDATA will disappear when converting XML to XMQ. Thus documents containing CDATA,
    converted to XMQ and back TO XML will have lost their CDATA nodes. Still the content
    will be properly encoded using normal XML escapes, instead of CDATA.</li>
    <li>XML permits two types of quotation marks. XMQ defaults to " when generating XML,
    but might switch to ' for content containing ". Thus XMQ might not use the same quote character
    as the original XML.</li>
    <li>Leading/ending whitespace of content, is by default trimmed when loading XML for conversion into XMQ.
    To preserve whitespace add <b>-p</b></li>
    </ol>
    </p>

    <p>To avoid cluttering the examples below with xml declarations/html5 doctypes, <b>--nodec</b> is used.</p>

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
        OPTIONS=$(echo "$line" | cut -f 4 -d ' ')
        OPTIONSS=$(echo "$line" | cut -f 5 -d ' ')
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

        testcase $FROM $TO BOTH "$OPTIONS" "$OPTIONSS"

    elif [[ $line == RIGHT* ]]
    then
        FROM=$(echo "$line" | cut -f 2 -d ' ')
        TO=$(echo "$line" | cut -f 3 -d ' ')
        OPTIONS=$(echo "$line" | cut -f 3 -d ' ')
        OPTIONSS=$(echo "$line" | cut -f 4 -d ' ')

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

        testcase $FROM $TO RIGHT "$OPTIONS" "$OPTIONSS"
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
