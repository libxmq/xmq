<?xml version="1.0"?>
<!-- UTF-16 output (with a byte order mark), a cdata section and disable-output-escaping. -->
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output encoding="UTF-16" indent="yes" cdata-section-elements="c" standalone="yes"/>
  <xsl:template match="/">
    <doc><c>a&lt;b &#x1F600;</c><t><xsl:value-of select="'&lt;raw/&gt;'" disable-output-escaping="yes"/></t></doc>
  </xsl:template>
</xsl:stylesheet>
