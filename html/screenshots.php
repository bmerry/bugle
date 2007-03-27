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
    <dl>
        <dt>Statistics display</dt>
        <dd><img src="images/stats.png" alt="Demo of the statistics display" /></dd>

        <dt>GUI debugger</dt>
        <dd><img src="images/gldb-gui1.png" alt="gldb-gui state" /></dd>
        <dd><img src="images/gldb-gui2.png" alt="gldb-gui texture display" /></dd>
        <dd><img src="images/gldb-gui3.png" alt="gldb-gui shader display" /></dd>
        <dd><img src="images/gldb-gui4.png" alt="gldb-gui backtrace" /></dd>
    </dl>
    <hr />
    <p>
    <a href="http://sourceforge.net"><img 
        src="http://sourceforge.net/sflogo.php?group_id=110905&amp;type=1" 
        width="88" height="31" alt="SourceForge.net Logo" /></a>
    </p>
<?php
if (file_exists("/home/virtual/opengl.org/var/www/html/sdk/inc/sdk_footer.txt"))
    include("/home/virtual/opengl.org/var/www/html/sdk/inc/sdk_footer.txt");
else
    include("sdk_footer.txt");
?>
</body>
</html>

