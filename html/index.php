<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en">
<head>
    <title>BuGLe: an OpenGL debugging tool</title>
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
    <h1>About BuGLe</h1>
    <p>
    BuGLe is a tool for OpenGL debugging, implemented as a wrapper library
    that sits between your program and OpenGL. While still in
    development, it can already do the following:
    </p>
    <ul>
        <li>Dump a textual log of all GL calls made.</li>
        <li>Take a screenshot or capture a video.</li>
        <li>Call <tt>glGetError</tt> after each call to check for errors, and
        wrap <tt>glGetError</tt> so that this checking is transparent to your
        program.</li>
        <li>Capture and display statistics (such as frame rate)</li>
        <li>Force a wireframe mode</li>
        <li>Recover a backtrace from segmentation faults inside the driver, even
        if the driver is compiled without symbols.</li>
    </ul>

    <p>In addition, there is a debugger (<kbd>gldb</kbd>) that lets you set
    breakpoints and examine backtraces. It also lets you examine OpenGL
    state, enable and disable filters, and drop into gdb to
    see what is going wrong. A GUI version of the debugger (<kbd>gldb-gui</kbd>)
    is currently in development, and a preliminary version is available
    with the latest releases. It features a texture viewer and a shader viewer.
    </p>

    <p>
    You can download the source from the
    <a href="http://sourceforge.net/projects/bugle/">project page</a>,
    or view the <a href="documentation/index.php">documentation</a>
    online. BuGLe is also available in Gentoo portage and the BSD ports
    collection.
    </p>

    <hr />

    <h2>Requirements</h2>
    <ul>
        <li><a href="http://gcc.gnu.org">GCC</a> 3.2 or later (4.0
        is <a href="http://gcc.gnu.org/bugzilla/show_bug.cgi?id=18279">broken</a>, but 4.1 works).</li>
        <li><a href="http://ffmpeg.sourceforge.net/">FFmpeg</a> is needed for
        video capture</li>
        <li><a href="http://cnswww.cns.cwru.edu/~chet/readline/rltop.html">GNU
            readline</a> is recommended for history editing in <kbd>gldb</kbd></li>
        <li><a href="http://www.gtk.org/">GTK+</a> is required for <kbd>gldb-gui</kbd></li>
        <li><a href="http://gtkglext.sourceforge.net/">GtkGLExt</a> is highly
        recommended for <kbd>gldb-gui</kbd> (without it, the texture display will
        not work)</li>
    </ul>

    <hr />
    <h2>Example</h2>
    <p>
    Here is an extract from a log, generated from an application I am writing.
    Note that <tt>GLenum</tt>s are displayed by name, and pointers are followed
    to the correct number of elements.
    </p>
<blockquote><tt>
stats.fps: 22.671<br />
stats.fragments: 52335<br />
stats.triangles: 99732<br />
trace.call: glXSwapBuffers(0x8117720, 0x01c00021)<br />
trace.call: glXMakeCurrent(0x8117720, 0x01c00021, 0x8444800) = 1<br />
trace.call: glBindBufferARB(GL_ARRAY_BUFFER, 1)<br />
trace.call: glMapBufferARB(GL_ARRAY_BUFFER, GL_READ_WRITE) = 0x45c3c000<br />
trace.call: glUnmapBufferARB(GL_ARRAY_BUFFER) = GL_TRUE<br />
trace.call: glBindBufferARB(GL_ARRAY_BUFFER, 0)<br />
trace.call: glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)<br />
trace.call: glLoadMatrixd(0xbfffe610 -&gt; { { 0, -0.29661, 1.22295, 0 }, { 1.22295, 0, 0, 0 }, { 0, 1.18644, 0.305739, 0 }, { 0.037888, 1.61781, -1.52576, 1 } })<br />
trace.call: glActiveTextureARB(GL_TEXTURE0)<br />
trace.call: glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, 0xbfffe5d0 -&gt; { 0.778524, 0.778524, 0.569631, 0 })<br />
trace.call: glGetIntegerv(GL_MAX_TEXTURE_UNITS, 0xbfffe688 -&gt; 4)<br />
trace.call: glBindBufferARB(GL_ARRAY_BUFFER, 1)<br />
trace.call: glVertexPointer(3, GL_FLOAT, 32, (nil))<br />
</tt></blockquote>

    <hr />
    <p>
    <a href="http://sourceforge.net/projects/bugle"><img src="http://sflogo.sourceforge.net/sflogo.php?group_id=110905&amp;type=12" width="120" height="30" border="0" alt="Get BuGLe at SourceForge.net. Fast, secure and Free Open Source software downloads" /></a>
<?php
if (file_exists("/home/virtual/opengl.org/var/www/html/sdk/inc/sdk_footer.txt"))
    include("/home/virtual/opengl.org/var/www/html/sdk/inc/sdk_footer.txt");
else
    include("sdk_footer.txt");
?>
</body>
</html>
