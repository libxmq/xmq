#!/bin/bash

TODAY=$(date +'%Y-%m-%d %H:%M')
XMQ=build/default/release/xmq

mkdir -p build/web/resources
$XMQ web/50x.htmq to-html > build/web/50x.html
$XMQ web/404.htmq to-html > build/web/404.html
cp doc/xmq.pdf  build/web
cp web/resources/style.css  build/web/resources
cp web/resources/code.js  build/web/resources
cp web/resources/shiporder.xml  build/web/resources/shiporder.xml
cp web/resources/car.xml  build/web/resources/car.xml
cp web/resources/welcome_traveller.htmq  build/web/resources/welcome_traveller.htmq
cp web/resources/welcome_traveller.html  build/web/resources/welcome_traveller.html
cp web/resources/sugar.xmq  build/web/resources/sugar.xmq
# Extract the css
$XMQ web/resources/shiporder.xml render-html --onlystyle > build/web/resources/xmq.css


# shiporder.xml
$XMQ web/resources/shiporder.xml to-xmq > build/web/resources/shiporder.xmq
$XMQ web/resources/shiporder.xml to-json | jq . > build/web/resources/shiporder.json
$XMQ web/resources/shiporder.xml render-html --id=ex1 --class=w40 --lightbg --nostyle  > build/rendered_shiporder_xmq.xml
$XMQ web/resources/shiporder.xml to-xmq --compact > build/shiporder_compact.xmq
$XMQ web/resources/shiporder.xml render-html --compact --id=ex1c --class=w40 --lightbg --nostyle  > build/rendered_shiporder_compact_xmq.xml


# config.xmq
cp web/resources/config.xmq  build/web/resources/
$XMQ web/resources/config.xmq render-html --class=w40 --lightbg --nostyle  > build/rendered_config_xmq.xml
$XMQ web/resources/config.xmq render-html --compact --class=w40 --lightbg --nostyle  > build/rendered_config_compact_xmq.xml
$XMQ --root=myconf web/resources/config.xmq to-xml  > build/config.xml

# multi.xmq --- multiple line comments
cp web/resources/multi.xmq build/web/resources
$XMQ web/resources/multi.xmq render-html --class=w40 --lightbg --nostyle  > build/rendered_multi_xmq.xml
$XMQ web/resources/multi.xmq render-html --class=w40 --lightbg --nostyle --compact  > build/rendered_multi_compact_xmq.xml
$XMQ web/resources/multi.xmq to-xml > build/multi.xml

# car.xml
$XMQ web/resources/car.xml render-html --class=w40 --lightbg --nostyle  > build/rendered_car_xmq.xml
$XMQ web/resources/car.xml to-xml  > build/web/resources/car.xml
$XMQ web/resources/car.xml to-xmq > build/web/resources/car.xmq

# sugare.xmq
echo -n "<span>" > build/sugar_xmq.xml
$XMQ web/resources/sugar.xmq tokenize --type=html >> build/sugar_xmq.xml
echo -n "</span>" >> build/sugar_xmq.xml

# Raw xml from corners and syntactic sugar.
$XMQ web/resources/syntactic_sugar.xmq tokenize --type=html > build/syntactic_sugar_xmq.xml
$XMQ web/resources/corners.xmq tokenize --type=html > build/corners_xmq.xml
$XMQ web/resources/compound.xmq tokenize --type=html > build/compound_xmq.xml

# todos.json
$XMQ web/resources/todos.json to-xmq > build/web/resources/todos.xmq
$XMQ web/resources/todos.json render-html --class=w80 --lightbg --nostyle > build/todos_json.xml
cp web/resources/todos.json build/web/resources/todos.json
cp web/resources/todos.xslt build/web/resources/todos.xslt
$XMQ web/resources/todos.xslt to-xmq > build/web/resources/todos.xslq
$XMQ web/resources/todos.xslt render-html --class=w80 --lightbg --nostyle > build/todos_xslq.xml
$XMQ web/resources/todos.json transform web/resources/todos.xslt to-html > build/web/resources/todos.html
$XMQ build/web/resources/todos.html render-html --class=w80 --darkbg --nostyle > build/todos_htmq.xml

# Render the welcome traveller xmq in html
$XMQ web/resources/welcome_traveller.htmq render-html --id=ex2 --class=w40 --lightbg --nostyle  > build/rendered_welcome_traveller_xmq.xml
$XMQ web/resources/welcome_traveller.html render-html --id=ex2 --class=w40 --lightbg --nostyle  > build/rendered_welcome_traveller_back_xmq.xml
$XMQ --trim=none web/resources/welcome_traveller.html to-htmq --escape-non-7bit | $XMQ - render-html --id=ex2 --class=w40 --lightbg --nostyle  > build/rendered_welcome_traveller_back_notrim_xmq.xml

# Render the same but compact
$XMQ web/resources/welcome_traveller.htmq render-html --id=ex2 --class=w40 --lightbg --nostyle --compact > build/rendered_welcome_traveller_xmq_compact.xml
$XMQ web/resources/welcome_traveller.htmq to-htmq > build/web/resources/welcome_traveller.htmq
$XMQ web/resources/welcome_traveller.htmq to-html > build/welcome_traveller_nopp.html
$XMQ pom.xml render-html --id=expom --class=w80 --lightbg --nostyle > build/pom_rendered.xml

$XMQ web/index.htmq \
     replace-entity DATE "$TODAY" \
     replace-entity SHIPORDER_XML --with-text-file=web/resources/shiporder.xml \
     replace-entity SHIPORDER_JSON --with-text-file=build/web/resources/shiporder.json \
     replace-entity SHIPORDER_XMQ --with-file=build/rendered_shiporder_xmq.xml \
     replace-entity SHIPORDER_COMPACT_XMQ --with-file=build/rendered_shiporder_compact_xmq.xml \
     replace-entity CONFIG_XMQ --with-file=build/rendered_config_xmq.xml \
     replace-entity CONFIG_COMPACT_XMQ --with-file=build/rendered_config_compact_xmq.xml \
     replace-entity CONFIG_XML --with-text-file=build/config.xml \
     replace-entity MULTI_XMQ --with-file=build/rendered_multi_xmq.xml \
     replace-entity MULTI_COMPACT_XMQ --with-file=build/rendered_multi_compact_xmq.xml \
     replace-entity MULTI_XML --with-text-file=build/multi.xml \
     replace-entity CAR_XML --with-text-file=web/resources/car.xml \
     replace-entity CAR_XMQ --with-file=build/rendered_car_xmq.xml \
     replace-entity CORNERS_XMQ --with-file=build/corners_xmq.xml \
     replace-entity COMPOUND_XMQ --with-file=build/compound_xmq.xml \
     replace-entity TODOS_XMQ --with-file=build/todos_json.xml \
     replace-entity TODOS_JSON --with-text-file=build/web/resources/todos.json \
     replace-entity TODOS_XSLQ --with-file=build/todos_xslq.xml \
     replace-entity TODOS_XSLT --with-text-file=build/web/resources/todos.xslt \
     replace-entity TODOS_HTMQ --with-file=build/todos_htmq.xml \
     replace-entity SYNTACTIC_SUGAR_XMQ --with-file=build/syntactic_sugar_xmq.xml \
     replace-entity WELCOME_TRAVELLER_HTMQ --with-file=build/rendered_welcome_traveller_xmq.xml \
     replace-entity WELCOME_TRAVELLER_BACK_HTMQ --with-file=build/rendered_welcome_traveller_back_xmq.xml \
     replace-entity WELCOME_TRAVELLER_BACK_NOTRIM_HTMQ --with-file=build/rendered_welcome_traveller_back_notrim_xmq.xml \
     replace-entity WELCOME_TRAVELLER_HTML --with-text-file=build/web/resources/welcome_traveller.html \
     replace-entity WELCOME_TRAVELLER_HTMQ_COMPACT --with-file=build/rendered_welcome_traveller_xmq_compact.xml \
     replace-entity WELCOME_TRAVELLER_NOPP_HTML --with-text-file=build/welcome_traveller_nopp.html \
     replace-entity POM_RENDERED --with-file=build/pom_rendered.xml \
     replace-entity XSLT_RENDERED --with-file=build/xslt_rendered.xml \
     to-html > build/web/index.html

# Render the page source itself!
$XMQ web/index.htmq render-html --darkbg > build/web/resources/index_htmq.html
$XMQ web/index.htmq render-html --lightbg > build/web/resources/index_htmq_light.html

echo Updated build/web/index.html
