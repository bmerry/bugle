<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
    <xsl:import href="http://docbook.sourceforge.net/release/xsl/current/xhtml/chunk.xsl"/>
    
    <xsl:param name="make.valid.html" select="'1'"/>
    <xsl:param name="html.stylesheet" select="'../bugle.css'"/>
    <xsl:param name="html.stylesheet.type" select="'text/css'"/>
    <xsl:param name="base.dir" select="'doc/html/man/'"/>
    <xsl:param name="img.src.path" select="'../images/'"/>
    <xsl:param name="use.id.as.filename" select="'1'"/>

    <!--
    <xsl:param name="admon.graphics.path" select="'images/'"/>
    <xsl:param name="admon.graphics.extension" select="'.png'"/>
    <xsl:param name="admon.graphics" select="'1'"/>
    -->
</xsl:stylesheet>
