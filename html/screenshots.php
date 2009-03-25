<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en">
<head>
    <title>BuGLe screenshots</title>
<?php
if (file_exists("/home/virtual/opengl.org/var/www/html/sdk/inc/sdk_head.txt"))
    include("/home/virtual/opengl.org/var/www/html/sdk/inc/sdk_head.txt");
else
    include("sdk_head.txt");
?>
</head>
<body>
<?php
if (file_exists("/home/virtual/opengl.org/var/www/html/sdk/inc/sdk_body_start.txt"))
    include("/home/virtual/opengl.org/var/www/html/sdk/inc/sdk_body_start.txt");
else
    include("sdk_body_start.txt");
?>
    <p>
    <a href="index.php">[About]</a>
    <a href="news.php">[News]</a>
    <a href="documentation/index.php">[Documentation]</a>
    <a href="screenshots.php">[Screenshots]</a>
    </p>
    <h1>Screenshots</h1>

    <ol>
        <li><a href="#statistics">Statistics</a></li>
        <li>Debugger</li>
        <li><ol>
            <li><a href="#gldb-gui-state">State</a></li>
            <li><a href="#gldb-gui-buffers">Buffers</a></li>
            <li><a href="#gldb-gui-textures">Textures</a></li>
            <li><a href="#gldb-gui-framebuffers">Framebuffers</a></li>
            <li><a href="#gldb-gui-shaders">Shaders</a></li>
            <li><a href="#gldb-gui-backtrace">Backtrace</a></li>
            <li><a href="#gldb-gui-breakpoints">Breakpoints</a></li>
        </ol></li>
    </ol>

    <h2><a name="statistics">Statistics display</a></h2>

    <img src="images/stats.png" alt="Demo of the statistics display" />

    <h2><a name="gldb-gui">GUI Debugger</a></h2>

    <p /><a name="gldb-gui-state"><img src="images/gldb-gui-state.png" alt="gldb-gui state view" /></a>
    <p /><a name="gldb-gui-buffers"><img src="images/gldb-gui-buffers.png" alt="gldb-gui buffers view" /></a>
    <p /><a name="gldb-gui-textures"><img src="images/gldb-gui-textures.png" alt="gldb-gui textures view" /></a>
    <p /><a name="gldb-gui-framebuffers"><img src="images/gldb-gui-framebuffers.png" alt="gldb-gui framebuffers view" /></a>
    <p /><a name="gldb-gui-shaders"><img src="images/gldb-gui-shaders.png" alt="gldb-gui shaders view" /></a>
    <p /><a name="gldb-gui-backtrace"><img src="images/gldb-gui-backtrace.png" alt="gldb-gui backtrace view" /></a>
    <p /><a name="gldb-gui-breakpoints"><img src="images/gldb-gui-breakpoints.png" alt="gldb-gui breakpoints view" /></a>

    <p /><a href="http://sourceforge.net/projects/bugle"><img src="http://sflogo.sourceforge.net/sflogo.php?group_id=110905&amp;type=12" width="120" height="30" border="0" alt="Get BuGLe at SourceForge.net. Fast, secure and Free Open Source software downloads" /></a>
<?php
if (file_exists("/home/virtual/opengl.org/var/www/html/sdk/inc/sdk_footer.txt"))
    include("/home/virtual/opengl.org/var/www/html/sdk/inc/sdk_footer.txt");
else
    include("sdk_footer.txt");
?>
</body>
</html>

