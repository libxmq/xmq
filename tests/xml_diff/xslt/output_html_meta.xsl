<?xml version="1.0"?>
<!-- html output: an existing content-type meta tag gets the encoding. -->
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output method="html" encoding="ISO-8859-1" doctype-public="-//W3C//DTD HTML 4.01//EN"/>
  <xsl:template match="/">
    <html><head><meta http-equiv="Content-Type" content="text/html; charset=utf-8"/><title>&#233;</title></head>
      <body><p>&#233;&#8364;<xsl:text disable-output-escaping="yes">&amp;nbsp;</xsl:text></p><input checked="checked" value=""/></body></html>
  </xsl:template>
</xsl:stylesheet>
