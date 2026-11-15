#!/bin/sh

if [ -z "$1" ]
then
    echo "Usage:"
    echo "fetch_and_build.sh libxml2      (only for the differential tests, make testdiff)"
    echo "fetch_and_build.sh libxslt      (only for the differential tests, make testdiff)"
    echo "fetch_and_build.sh invisiblexml"
    echo "fetch_and_build.sh ixml ixml/tests/correct/test-catalog.xml"
    exit 0
fi

DIR=$(pwd)

# All cases operate in (and create files in) the directory containing this
# script, so if it was invoked from somewhere else (e.g.
# 'sh 3rdparty/fetch_and_build.sh ...' from the repo root), move there.
case "$(pwd)" in
  *) case "$0" in
       */*) SDIR=$(cd "$(dirname "$0")" 2>/dev/null && pwd)
          if [ -n "$SDIR" ] && [ "$SDIR" != "$DIR" ]
          then
              cd "$SDIR" || exit 1
              DIR=$(pwd)
          fi ;;
     esac ;;
esac

NPROC=2
if command -v nproc > /dev/null 2>&1
then
    NPROC=$(nproc)
fi

if [ "$1" = "libxml2" ]
then
    if [ ! -d libxml2 ]; then
        echo
        echo Fetching libxml2
        echo
        git clone https://github.com/libxmq/libxml2.git
    fi

    # ./.libs/libxml2.a
    cd libxml2
    if [ ! -f ./.libs/libxml2.a ]; then
        echo
        echo Building libxml2
        echo

        autoreconf -fi
        ./autogen.sh --enable-static=yes --with-zlib=no --with-lzma=no --with-python=no --with-http=no
        make -j${NPROC}
    fi
    cd ..
    exit 0
fi

if [ "$1" = "libxslt" ]
then
    if [ ! -d libxslt ]; then
        echo
        echo Fetching libxslt
        echo
        git clone https://github.com/libxmq/libxslt.git
    fi

    cd libxslt
    if [ ! -f ./libxslt/.libs/libxslt.a ]; then
        echo
        echo Building static libxslt
        echo

        # Do this autogen twice to workaround problem with ltmain.sh missing.
        autoreconf -fi
        ./autogen.sh --enable-static=yes --with-libxml-src=${DIR}/libxml2/ --with-python=no || true
        ./autogen.sh --enable-static=yes --with-libxml-src=${DIR}/libxml2/ --with-python=no
        make -j${NPROC}
    fi
    cd ..

    exit 0
fi

if [ "$1" = "ixml" ]
then
    if [ ! -f ixml/README.md ]
    then
        #git clone https://codeberg.org/InvisibleXML/ixml.git
        git clone ssh://git@codeberg.org/weetmuts/ixml.git
    fi

    if [ ! -f saxon.jar ]
    then
        ( cd /tmp && mvn -q org.apache.maven.plugins:maven-dependency-plugin:3.8.1:copy \
              -Dartifact=net.sf.saxon:Saxon-HE:13.1 \
              -DoutputDirectory=$DIR \
          && mv -f Saxon-HE-13.1.jar saxon.jar )
    fi

    # Saxon >= 12 also needs xmlresolver on the classpath.
    # Fetch from a neutral directory so the local pom.xml (saxon-download)
    # is not picked up; mvn names the artifact version-suffixed, so rename.
    if [ ! -f xmlresolver.jar ]
    then
        ( cd /tmp && mvn -q org.apache.maven.plugins:maven-dependency-plugin:3.8.1:copy \
              -Dartifact=org.xmlresolver:xmlresolver:6.0.10 \
              -DoutputDirectory=$DIR \
          && mv -f xmlresolver-6.0.10.jar xmlresolver.jar )
    fi

    CAT=$2
    if [ -z "$CAT" ] || { [ ! -f "$CAT" ] && [ ! -f "$DIR/$CAT" ]; }
    then
        echo "Usage: ./fetch_and_build.sh ixml <test-catalog>"
        echo "       e.g. ./fetch_and_build.sh ixml ixml/tests/correct/test-catalog.xml"
        echo "       (<test-catalog> may be absolute, or found from the current"
        echo "        working directory, or relative to this script's directory>)"
        exit 1
    fi
    if [ ! -f "$CAT" ]
    then
        CAT=$DIR/$CAT
    fi

    # Generate the test harness bash script (run-tests.sh) from the iXML
    # test catalog, using Saxon and ixml/tests/tools/testcat-to-bash.xsl.
    # N.B. that stylesheet does not follow tc:test-set-ref, so the
    # top-level catalog yields report-boilerplate only; point -s: at a
    # set's own catalog (e.g. ixml/tests/correct/test-catalog.xml) or
    # ixml/tests/ixml/test-catalog.xml to get real test invocations.
    # The generated script calls $CMD (default: 'ixml') as
    # 'CMD input grammar output'; set CMD=<wrapper> to use xmq instead.
    PROCVER=
    BUILDDIR=$(ls -d $DIR/../build/*/release 2>/dev/null | head -n 1)
    if [ -n "$BUILDDIR" ] && [ -x "$BUILDDIR/xmq" ]
    then
        PROCVER=$("$BUILDDIR/xmq" --version 2> /dev/null | head -n 1 | sed 's/^xmq:[[:space:]]*//')
    fi
    [ -n "$PROCVER" ] || PROCVER=unknown

    java -cp $DIR/saxon.jar:$DIR/xmlresolver.jar net.sf.saxon.Transform \
        -s:$CAT \
        -xsl:$DIR/ixml/tests/tools/testcat-to-bash.xsl \
        CMD=./xmqwrapper.sh PROCESSOR=xmq "PROCVER=$PROCVER" \
        -o:$DIR/run-tests.sh

    chmod a+x $DIR/run-tests.sh

    # The reference harness also materializes inline <ixml-grammar> bodies
    # and inline <test-string> inputs from the catalog into
    # $PROCESSOR-results/ before running, and testcat-to-bash.xsl assumes
    # that (it references $PROCESSOR-results/grammar.<set>.ixml and
    # input.<set>...<case>.txt).  So do it here: dump them as raw text,
    # preserving characters such as CRs verbatim.
    mkdir -p $DIR/xmq-results
    IXMAT=$(mktemp /tmp/ixmat.XXXXXX)
    cat > "$IXMAT" << 'IXMAT_XSL'
<xsl:stylesheet version="3.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:tc="http://invisiblexml.org/NS/test-catalog">
  <xsl:template match="/">
    <xsl:for-each select="//tc:test-set[tc:ixml-grammar]">
      <xsl:result-document
          href="{concat('xmq-results/grammar.', string(@name), '.ixml')}"
          method="text" encoding="utf-8">
        <xsl:value-of select="tc:ixml-grammar"/>
      </xsl:result-document>
    </xsl:for-each>
    <xsl:for-each select="//tc:test-case[tc:test-string]">
      <xsl:result-document
          href="{concat('xmq-results/input.', string(parent::tc:test-set/@name),
                        '...', string(@name), '.txt')}"
          method="text" encoding="utf-8">
        <xsl:value-of select="tc:test-string"/>
      </xsl:result-document>
    </xsl:for-each>
  </xsl:template>
</xsl:stylesheet>
IXMAT_XSL
    java -cp $DIR/saxon.jar:$DIR/xmlresolver.jar net.sf.saxon.Transform \
        -s:$CAT -xsl:"$IXMAT" > /dev/null \
    || echo "warning: could not materialize inline grammars/inputs"
    rm -f "$IXMAT"

    # The generated script calls './xmqwrapper.sh' with a path relative to
    # the working directory, so run it from this directory.  $PROCESSOR
    # decides the results directory (xmq-results).
    ( cd "$DIR" && PROCESSOR=xmq sh ./run-tests.sh )

    # Render the latest XML test report as a self-contained HTML page.
    REPORT=$(ls -t "$DIR"/xmq-results/test-report.*.xml 2> /dev/null | head -n 1)
    if [ -n "$REPORT" ]
    then
        HTML=${REPORT%.xml}.html
        java -cp $DIR/saxon.jar:$DIR/xmlresolver.jar net.sf.saxon.Transform \
            -s:"$REPORT" \
            -xsl:$DIR/ixml/tests/tools/test-report-to-html.xsl \
            -o:"$HTML" \
        && echo "HTML report: $HTML" \
        || echo "warning: could not render HTML report ($REPORT)"
    fi

    exit 0
fi
