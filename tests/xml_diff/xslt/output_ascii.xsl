<?xml version="1.0"?>
<!-- US-ASCII output: non ascii characters become character references, also in attributes and comments. -->
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output encoding="US-ASCII" omit-xml-declaration="no" doctype-system="doc.dtd"/>
  <xsl:template match="/">
    <xsl:comment>k&#246;k</xsl:comment>
    <doc a="&#246;&#8364;">&#246;&#xE000;<?pi &#246;?></doc>
  </xsl:template>
</xsl:stylesheet>
