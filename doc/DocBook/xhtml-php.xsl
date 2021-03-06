<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
    <xsl:param name="html.ext" select="'.php'"/>

    <xsl:template name="user.head.content">
        <xsl:processing-instruction name="php">
if (file_exists("/home/virtual/opengl.org/var/www/html/sdk/inc/sdk_head.txt"))
    include("/home/virtual/opengl.org/var/www/html/sdk/inc/sdk_head.txt");
else
    include("../sdk_head.txt");
        </xsl:processing-instruction>
    </xsl:template>

    <xsl:template name="user.header.navigation">
        <xsl:processing-instruction name="php">
if (file_exists("/home/virtual/opengl.org/var/www/html/sdk/inc/sdk_body_start.txt"))
    include("/home/virtual/opengl.org/var/www/html/sdk/inc/sdk_body_start.txt");
else
    include("../sdk_body_start.txt");
        </xsl:processing-instruction>
        <p>
            <a href="../index.php">[About]</a><xsl:text> </xsl:text>
            <a href="../news.php">[News]</a><xsl:text> </xsl:text>
            <a href="index.php">[Documentation]</a><xsl:text> </xsl:text>
            <a href="../screenshots.php">[Screenshots]</a><xsl:text> </xsl:text>
        </p>
    </xsl:template>

    <xsl:template name="user.footer.navigation">
        <hr />
        <p>
            <a href="http://sourceforge.net/projects/bugle"><img src="http://sflogo.sourceforge.net/sflogo.php?group_id=110905&amp;type=12" width="120" height="30" border="0" alt="Get BuGLe at SourceForge.net. Fast, secure and Free Open Source software downloads" /></a>
        </p>
        <xsl:processing-instruction name="php">
if (file_exists("/home/virtual/opengl.org/var/www/html/sdk/inc/sdk_footer.txt"))
    include("/home/virtual/opengl.org/var/www/html/sdk/inc/sdk_footer.txt");
else
    include("../sdk_footer.txt");
        </xsl:processing-instruction>
    </xsl:template>
</xsl:stylesheet>
