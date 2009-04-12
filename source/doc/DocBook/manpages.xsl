<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
    <xsl:import href="http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl"/>
    <xsl:param name="use.id.as.filename" select="'1'"/>
    <xsl:param name="man.output.in.separate.dir" select="'1'"/>
    <xsl:param name="man.output.base.dir" select="'doc/'"/>
    <xsl:param name="man.output.subdirs" select="'1'"/>
    <xsl:param name="man.authors.section.enabled" select="'0'"/>
    <xsl:param name="man.copyright.section.enabled" select="'0'"/>
    <xsl:param name="man.charmap.enabled" select="'1'"/>
    <xsl:param name="man.charmap.use.subset" select="'1'"/>
    <xsl:param name="man.charmap.subset.profile">
        @*[local-name() = 'name'] = 'RIGHTWARDS ARROW'
    </xsl:param>

    <!-- man.authors.section.enabled requires a recent stylesheet -->
    <xsl:template name="author.section"/>
    <xsl:template name="copyright.section"/>

    <!-- insert the version number from a command-line parameter -->
    <xsl:template name="get.refentry.version">
        <xsl:value-of select="$bugle.version"/>
    </xsl:template>
</xsl:stylesheet>
