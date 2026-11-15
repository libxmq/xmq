<?xml version="1.0"?>
<!-- xsl:document (XSLT 1.1 draft) and the output parameters of the written documents. -->
<xsl:stylesheet version="1.1" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output method="xml" indent="yes"/>
  <xsl:template match="/">
    <xsl:document href="out_xml.xml" indent="yes" encoding="ISO-8859-1">
      <doc><x>&#229;&#8364;</x><xsl:comment>c</xsl:comment></doc>
    </xsl:document>
    <xsl:document href="out_html.html" method="html">
      <html><head><title>T</title></head><body><p>&#229;<br/></p></body></html>
    </xsl:document>
    <xsl:document href="out_text.txt" method="text"><xsl:value-of select="/doc/@a"/>&#10;</xsl:document>
    <xsl:document href="out_xhtml.xml" doctype-public="-//W3C//DTD XHTML 1.0 Strict//EN"
                  doctype-system="http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
      <html><head><title>X</title></head><body><p lang="en">a<br/><img src="i"/></p><a name="n">l</a></body></html>
    </xsl:document>
    <done/>
  </xsl:template>
</xsl:stylesheet>
