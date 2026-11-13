# Cases for tests/test_cli_grammar.sh: check "expected line" "expected tree" args...
# Print the actual values for new cases with: tests/test_cli_grammar.sh "" "" --print

check xmq\ -z\ add-root\ gurka\ to-json xmq\{option\(name=no-input\)command\(name=add-root\)\{arg=gurka\}command\(name=to-json\)\} -z add-root gurka to-json
check xmq\ --trim=none\ index.html\ delete\ //script\ select\ -c\ //a\[@x=1\]\ to-html xmq\{option\(name=trim\ value=none\)file=index.html\ command\(name=delete\)\{arg=\'//script\'\}command\(name=select\)\{option\(name=compact\)arg=\'//a\[@x=1\]\'\}command\(name=to-html\)\} --trim=none index.html delete //script select -c //a\[@x=1\] to-html
check xmq\ foo\ bar\ \"x\ \\\"y\" ixml\(ixml:state=failed\ xmlns:ixml=http://invisiblexml.org/NS\)\{info=\(\'ixml:1:8:\ syntax\ error\'\&#10\;\'xmq\ foo\ bar\ \"x\ \\\"y\"\'\&#10\;\'\ \ \ \ \ \ \ \^\'\)line=1\ column=8\ pos=8\} foo bar x\ \"y
check xmq\ pom.xml\ to-json\ -p xmq\{file=pom.xml\ command\(name=to-json\)\{option\(name=pretty\)\}\} pom.xml to-json -p
check xmq\ add\ h1=Welcome\ add\ p=Howdy\ to-xmq xmq\{command\(name=add\)\{arg=h1=Welcome\}command\(name=add\)\{arg=p=Howdy\}command\(name=to-xmq\)\} add h1=Welcome add p=Howdy to-xmq
check xmq\ input.xmq\ replace-entity\ id\ 4711\ replace-entity\ color\ red\ replace-entity\ welcome\ \"Welcome\ stranger\!\"\ to-xmq\ --compact xmq\{file=input.xmq\ command\(name=replace-entity\)\{arg=id\ arg=4711\}command\(name=replace-entity\)\{arg=color\ arg=red\}command\(name=replace-entity\)\{arg=welcome\ arg=\'Welcome\ stranger\!\'\}command\(name=to-xmq\)\{option\(name=compact\)\}\} input.xmq replace-entity id 4711 replace-entity color red replace-entity welcome Welcome\ stranger\! to-xmq --compact
check xmq\ input.xmq\ replace\ /http/name=server\ replace\ /http/port=44\ to-xmq\ --compact xmq\{file=input.xmq\ command\(name=replace\)\{arg=/http/name=server\}command\(name=replace\)\{arg=/http/port=44\}command\(name=to-xmq\)\{option\(name=compact\)\}\} input.xmq replace /http/name=server replace /http/port=44 to-xmq --compact
check xmq\ in.xml\ for-each\ /enter/test/name\ --shell=\"echo\ \$\{..\}=\\\"\$\{.\}\\\"\" xmq\{file=in.xml\ command\(name=for-each\)\{arg=/enter/test/name\ option\(name=shell\ value=\'echo\ \$\{..\}=\"\$\{.\}\"\'\)\}\} in.xml for-each /enter/test/name --shell=echo\ \$\{..\}=\"\$\{.\}\"
check xmq\ plumbing_products.html\ select\ count\(//td\)\ to-text xmq\{file=plumbing_products.html\ command\(name=select\)\{arg=\'count\(//td\)\'\}command\(name=to-text\)\} plumbing_products.html select count\(//td\) to-text
check xmq\ ixml:data/csv\ tests/samples/plumbing_products.csv\ xsl:data/table-to-web\ to-html xmq\{grammar=ixml:data/csv\ file=tests/samples/plumbing_products.csv\ command\(name=xsl\)\{arg=data/table-to-web\}command\(name=to-html\)\} ixml:data/csv tests/samples/plumbing_products.csv xsl:data/table-to-web to-html
check xmq\ library/core/ixml.ixml\ library/core/ixml.ixml xmq\{grammar=library/core/ixml.ixml\ file=library/core/ixml.ixml\} library/core/ixml.ixml library/core/ixml.ixml
check xmq\ natlang.ixml\ natlang.inp xmq\{grammar=natlang.ixml\ file=natlang.inp\} natlang.ixml natlang.inp
check xmq\ grammar.ixml xmq\{file=grammar.ixml\} grammar.ixml
check xmq\ --lines\ --ixml-fail-silent\ grammar.ixml\ input.xmq xmq\{option\(name=lines\)option\(name=ixml-fail-silent\)grammar=grammar.ixml\ file=input.xmq\} --lines --ixml-fail-silent grammar.ixml input.xmq
check xmq\ --ixml=t.ixml\ -z xmq\{option\(name=ixml\ value=t.ixml\)option\(name=no-input\)\} --ixml=t.ixml -z
check xmq\ load xmq\{command\(name=load\)\} load
check xmq\ load\ data.xml\ to-xml xmq\{command\(name=load\)\{arg=data.xml\}command\(name=to-xml\)\} load data.xml to-xml
check xmq\ -i\ \"body\(name=1\)\{content=\'\'\'alfa\'\'\'\}\"\ select\ //body/content xmq\{input=\"body\(name=1\)\{content=\'\'\'alfa\'\'\'\}\"command\(name=select\)\{arg=\'//body/content\'\}\} -i body\(name=1\)\{content=\'\'\'alfa\'\'\'\} select //body/content
check $'xmq -i "alfa\nbeta"' xmq\{input=\(\'alfa\'\&#10\;\'beta\'\)\} -i $'alfa\nbeta'
check xmq\ -\ to-xmq xmq\{file=-\ command\(name=to-xmq\)\} - to-xmq
check xmq\ render-tex xmq\{command\(name=render-tex\)\} render-tex
check xmq\ t.xmq\ tokenize\ --type=debugtokens xmq\{file=t.xmq\ command\(name=tokenize\)\{option\(name=type\ value=debugtokens\)\}\} t.xmq tokenize --type=debugtokens
check xmq\ in.xml\ transform\ --stringparam=\"title=Hello\ World\"\ cols.xslq\ to-text xmq\{file=in.xml\ command\(name=transform\)\{option\(name=stringparam\ value=\'title=Hello\ World\'\)arg=cols.xslq\}command\(name=to-text\)\} in.xml transform --stringparam=title=Hello\ World cols.xslq to-text
check xmq\ t.xmq\ select\ -x\ --\ -x xmq\{file=t.xmq\ command\(name=select\)\{option\(name=x\)arg=-x\}\} t.xmq select -x -- -x
check xmq\ t.xmq\ select\ --\ -x xmq\{file=t.xmq\ command\(name=select\)\{arg=-x\}\} t.xmq select -- -x
check xmq\ help xmq\{command\(name=help\)\} help
check xmq\ help\ to-xml xmq\{command\(name=help\)\{arg=to-xml\}\} help to-xml
check xmq\ --help xmq\{option\(name=help\)\} --help
check xmq\ -h xmq\{option\(name=help\)\} -h
check xmq\ t.xml\ to-xml\ -o\ -n xmq\{file=t.xml\ command\(name=to-xml\)\{option\(name=omit-decl\)option\(name=no-final-nl\)\}\} t.xml to-xml -o -n
check xmq\ \"my\ file.xml\"\ to-xmq xmq\{file=\'my\ file.xml\'command\(name=to-xmq\)\} my\ file.xml to-xmq
check xmq\ t.xml\ save-to\ out.xmq xmq\{file=t.xml\ command\(name=save-to\)\{arg=out.xmq\}\} t.xml save-to out.xmq
check xmq\ t.xml\ validate\ --silent\ schema.xsd xmq\{file=t.xml\ command\(name=validate\)\{option\(name=silent\)arg=schema.xsd\}\} t.xml validate --silent schema.xsd
check xmq\ t.xml\ pa xmq\{file=t.xml\ command\(name=pa\)\} t.xml pa
check xmq\ t.xml\ page xmq\{file=t.xml\ command\(name=page\)\} t.xml page
check xmq\ t.xml\ to-htmq\ --escape-non-7bit xmq\{file=t.xml\ command\(name=to-htmq\)\{option\(name=escape-non-7bit\)\}\} t.xml to-htmq --escape-non-7bit
check xmq\ t.xml\ --name=\ to-xmq xmq\{file=t.xml\ option\(name=name\ value\)command\(name=to-xmq\)\} t.xml --name= to-xmq
check xmq\ \"back\\\\slash.xml\" xmq\{file=back\\slash.xml\} back\\slash.xml
check xmq\ t.xml\ select\ \"\" xmq\{file=t.xml\ command\(name=select\)\{arg\}\} t.xml select ''
check xmq\ t.xmq\ select\ -s\ --gofast\ something xmq\{file=t.xmq\ command\(name=select\)\{option\(name=s\)option\(name=gofast\)arg=something\}\} t.xmq select -s --gofast something
check xmq\ -lx\ -l\ -lxx\ -zz\ -q\ t.xml\ to-xml\ -co\ -x xmq\{option\(name=log-xmq\)option\(name=l\)option\(name=lxx\)option\(name=zz\)option\(name=q\)file=t.xml\ command\(name=to-xml\)\{option\(name=co\)option\(name=x\)\}\} -lx -l -lxx -zz -q t.xml to-xml -co -x
check xmq\ t.xmq\ replace-entity\ --a\ X\ --b\ Y\ --c xmq\{file=t.xmq\ command\(name=replace-entity\)\{option\(name=a\)arg=X\ option\(name=b\)arg=Y\ option\(name=c\)\}\} t.xmq replace-entity --a X --b Y --c
