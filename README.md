# xmq
Convert xml to a human readable/editable format and back.

Xml can be human readable/editable if it is used for
markup of longer human language texts, ie books, articles
and other documents etc. In these cases the xml-tags
represent a minor part of the whole xml-file.

However xml is often used for configuration files, the
most prevalent is the pom.xml files for maven.
In such config files the xml-tags represent a major part
of the whole xml-file. This makes the config files
hard to read and edit directly by hand.

The xmq format is simply a restructuring of the xml
that, to me at least, makes config files written
in xml more easily read and editable.

The xmq format exactly represents the xml format
and can therefore be converted back to xml after
any editing has been done. (Caveat whitespace
trimmings, but this can be configured.)

Type `xmq pom.xml > pom.xmq`
to convert your pom.xml file into something that is more easily
read/editable with for example emacs/vi.

Make your desired changes in the xml file and then
do `xmq pom.xmq > pom.xml` to convert it back.

You can read form stdin like this:  `cat pom.xml | xmq -`
and there is a utility to view xml files: `xmqless pom.xml`

Do `make && sudo make install` to have xmq and xmqless installed
into /usr/local/bin.

# Example

Xml to the left. Colored XMQ (using xmqless) to the right.

![XML vs XMQ](/doc/xml_vs_xmq.png)