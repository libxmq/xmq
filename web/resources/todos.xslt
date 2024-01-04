<xsl:stylesheet version="1.0"
xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="xml" omit-xml-declaration="yes" version="1.0" encoding="utf-8" indent="yes"/>

<xsl:template match="_/todos">
    <html>
    <body>
    <table border="1" >
        <xsl:for-each select="_">
            <tr>
                <td>
                    <xsl:value-of select="todo" />
                </td>
            </tr>
        </xsl:for-each>
    </table>
    </body>
    </html>
</xsl:template>

<xsl:template match="total">
</xsl:template>

<xsl:template match="skip">
</xsl:template>

<xsl:template match="limit">
</xsl:template>

</xsl:stylesheet>
