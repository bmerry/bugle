<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en">
<head>
    <title>BuGLe news</title>
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
    <h1>BuGLe News</h1>

    <h2>Release to fix compilation issues (01/08/2009)</h2>
    <p>The just-released 0.0.20090801 version adds a fix for GCC 4.4,
    which mysteriously duplicates function nodes in the parse tree. And in
    case you missed it (I didn't announce it here), the previous release also
    fixed a problem with recent versions of <tt>glext.h</tt>.
    </p>

    <p>Sourceforge have also enabled email notifications on
    <a href="https://apps.sourceforge.net/trac/bugle/">Trac</a>, so it should
    now be easier to get my attention when posting bugs there.
    </p>

    <h2>Mailing lists and other news (31/5/2009)</h2>
    <p>Sourceforge is planning to remove the forums feature soon, which will
    eliminate that avenue for support. I've never really liked the forums
    model, since it meant I couldn't just hit reply in my mail client, so I'm
    going to switch to mailing lists. There are now three mailing lists
    accessible through the <a
        href="http://sourceforge.net/projects/bugle/">project page</a> on
    Sourceforge:</p>
    <dl>
        <dt>bugle-announce</dt>
        <dd>For new release announcements</dd>
        <dt>bugle-users</dt>
        <dd>For general help and discussion about bugle</dd>
        <dt>bugle-devel</dt>
        <dd>For anyone interesting in hacking on the bugle core or writing new
        filter-sets</dd>
    </dl>

    <p>I've been fairly busy working on bugle lately, but I don't have much to
    show for it yet. I'm trying to implement a <a
        href="http://www.scons.org">scons</a> build system, which should in
    theory allow me to do ports to systems without a POSIX build environment,
    such as a WinCE port using the standard toolchains. It's proving to be a
    lot of work, since I've been using <a
        href="http://www.gnu.org/software/gnulib/">gnulib</a> to handle
    portability nastiness, and it's wrapped pretty tightly around autoconf and
    automake. I'm working on writing a portability abstraction layer, which
    will be able to use gnulib as a backend on systems where it makes sense,
    but also have other OS-specific backends.</p>

    <h2>New ticket system, new release (15/3/2009)</h2>
    <p>
    BuGLe has grown to the point that having people email me bug reports meant
    that they tended to get lost. I've set up a <a
        href="http://apps.sourceforge.net/trac/bugle">trac</a> system where
    you can report bugs in future.
    </p>
    <p>I also made a new release on 11 March, which contains a number of new
    features (including a VBO viewer) and bug fixes.
    </p>
    <h2>Bugfixes and features (21/2/2009)</h2>
    <p>
    This release features a whole host of new features and bugfixes. Most notable
    are experimental support for TCP/IP-based remote debugging and OpenGL ES 1.1.
    </p>
    <h2>Minor bugfixes (3/8/2008)</h2>
    <p>
    Today's release brings minor bugfixes in several areas, particularly in
    building for OpenGL ES and/or Windows.
    </p>
    <h2>Alpha support for Windows and OpenGL ES 2.0 (16/7/2008)</h2>
    <p>
    This is a major new release that adds experimental support for Windows and 
    for OpenGL ES 2.0. These ports are still in the early stages of development, 
    and many features don't yet work. Nevertheless, you're encouraged to try them 
    out.</p>
    <p>Note that you must delete the old contents of <kbd>$PREFIX/lib/bugle/*.so</kbd>
    when upgrading, as otherwise stale modules will be loaded into bugle.</p>
    <h2>Texture completeness checks</h2>
    <p>
    The new release adds partial support for checking whether textures are
    complete (in the <tt>checks</tt> filter-set), with warning messages
    printed when this is detected. It currently only works on 
    <h2>Developers invited to write their own filter-sets (3/1/2008)</h2>
    <p>
    There are a number of minor bug-fixes, but the major
    change in this release is that the filter-set API has been stabilised and made
    available to users through installed header files. If you are interested in
    writing your own filter-sets, refer to the
    <a href="documentation/extending.php">documentation</a>.
    </p>
    <p>
    I will also be starting full-time employment in February, and I won't be
    able to put as much effort in BuGLe as I have in the past. If anybody
    would like to become involved in developing BuGLe, contact me and I can
    suggest some things to work on while getting to know the way around the
    source code.
    </p>
    <h2>Remote debugging (6/12/2007)</h2>
    <p>
    This release has a lot of changes, but it's mostly under the hood to
    start bringing bugle to a point where 3rd parties can easily write and
    ship filter-sets. The most obvious change for users is that remote
    (network) debugging is now easier to do. See
    <a href="documentation/gldb-gui.1.php#gldb-gui-MAN-remote">gldb-gui(1)</a> for details
    (seriously, read it, as it's still very experimental and the best way
    isn't what the GUI might suggest).
    </p>
    <p>
    NB: if you are upgrading, you MUST delete
    <tt>/usr/local/lib/bugle/stats.so</tt>, otherwise you will get an error
    about cyclic dependencies on startup.
    </p>
    <h2>Redesigned documentation (7/11/2007)</h2>
    <p>This release fixes a number of compilation failures, crashes, and
    performance bugs. The manual pages have all been converted to DocBook,
    giving much higher quality HTML versions. Some improvements were made
    to the documentation at the same time.</p>
    <p>Some internals were redesigned, and so some new bugs may have been
    introduced. If you find any new bugs, please email me (bmerry
    users.sourceforge.net).</p>
    <h2>Improved performance startup code (10/10/2007)</h2>
    <p>I've done some work on reducing the overhead in bugle. The new release
    will be significantly faster when intercepting only a few functions.</p>
    <p>The startup code has also been reworked to handle programs that
    dynamically load OpenGL using <code>dlopen</code>.
    <h2>Geometry shader viewer, forums (18/7/2007)</h2>
    <p>It is now possible to view high-level geometry shaders in the shader
    viewer. There is also a minor bug-fix for the framebuffer viewer when
    using FBOs.</p>
    <p>I've also switched on the Sourceforge
    <a href="http://sourceforge.net/forum/?group_id=110905">forums</a>
    for the project.</p>
    <h2>Logging rewritten, extoverride filterset, bugfixes (29/5/2007)</h2>
    <p>
    The logging system has been heavily reworked. If you are upgrading,
    please read the manpage for
    <a href="documentation/man7/bugle-log.7.html">bugle-log(7)</a> and 
    <a href="documentation/man7/bugle-trace.7.html">bugle-trace(7)</a> and
    update your <tt>~/.bugle/filters</tt> as appropriate.
    </p>
    <p>
    There is also a new filter-set for hiding extensions
    (<a href="documentation/man7/bugle-extoverride.7.html">extoverride</a>)
    and miscellaneous bugfixes.
    </p>
    <h2>BuGLe now available in Gentoo portage (27/3/2007)</h2>
    <p>
    As a Gentoo user myself, I'm especially happy to announce that Gentoo
    is now available in the main portage tree (albeit with an unstable
    keyword). BuGLe is also already available in the BSD ports collection.
    </p>
    <h2>Support for some G80 extensions (25/3/2007)</h2>
    <p>
    I've added support for several of the new extensions exposed by
    the G80 (aka GeForce 8800); see the ChangeLog for details. These are
    not fully tested, specially where NVIDIA haven't released support in
    the drivers, so let me know if anything breaks.
    </p><p>
    The state viewer will also look a little different: some
    missing extension suffices have been added (e.g.,
    GL_TEXTURE_RECTANGLE_ARB was missing the _ARB) and the
    ordering has been changed to better mirror the order in the
    OpenGL 2.1 state tables.
    </p>
    <h2>New release fixes assorted bugs (17/2/2007)</h2>
    <p>This release fixes a number of bugs and makes some internal redesigns.
    The most visible new feature is that strings in the gldb-gui state
    viewer are now displayed unescaped, making it easier to read multi-line
    strings such as shader info logs.</p>
    <h2>View frustum, state dumping and bugfixes (7/1/2007)</h2>
    <p>The new release has several bug-fixes related to using gldb-gui with
    pbuffers and framebuffer objects. In addition, it is possible to save
    the current state to an XML file, and the camera filter-set has the
    option of drawing the original view frustum.</p>
    <h2>ATI fixes and range remapping (27/12/2006)</h2>
    <p>There is now an option in the texture and framebuffer viewers
    to remap the range. This is useful for viewing textures with data
    outside the [0, 1] range as well as images with low dynamic range.
    There are also many bugfixes; in particular, BuGLe should now work
    much better on ATI cards.</p>
    <h2>ccache quick-fix (10/11/2006)</h2>
    <p>Apparently I can't spell, because the ccache workaround in
    the previous release was broken. This release should fix it.
    If you've already managed to install 0.0.20061109, you don't
    need to upgrade, as only the Makefile.am was modified.</p>
    <h2>Video and screenshot capture fixed (9/11/2006)</h2>
    <p>Somehow I managed to break the screen capture code in
    the previous release (0.0.20061022) and didn't notice. I've
    fixed the bug and also made the code more robust, as well as
    enabling capture via a pixel buffer object without polluting
    the application's namespace.</p>

    <p>There are a few other bugfixes, mostly relating to the
    <kbd>validate</kbd> filter-set.</p>
    <h2>Statistics rewrite (22/10/2006)</h2>
    <p>The statistics system has been completely rewritten and is
    now a lot more powerful, supporting customisable statistics,
    graphs, batch counts, NVPerfSDK and lower overhead.</p>

    <p>You will need to update your <code>~/.bugle/filters</code>
    file to use any statistics (see the examples in
    <code>doc/examples/filters</code>), and you will also need to
    copy doc/examples/statistics to
    <code>~/.bugle/statistics</code>.</p>

    <p>You can see an example of the enhanced statistics on the <a
        href="screenshots.php">screenshots</a> page.</p>
    <h2>Miscellaneous improvements (13/09/2006)</h2>
    <p>
    While there is no one feature that makes this release special, it has a
    huge number of minor bug-fixes and improvements, mostly to the GUI
    debugger. See the ChangeLog for details.
    </p><p>
    The GUI debugger has almost achieved feature-parity with the text debugger
    (run-time enabling and disabling of filter-sets being one missing feature).
    I haven't really been maintaining the text debugger, and I'm thinking of
    discontinuing it. If there are reasons why you still prefer the text
    debugger, let me know soon so that I can reconsider.
    </p>
    <h2>Framebuffer viewer (28/08/2006)</h2>
    <p>
    gldb-gui now has a framebuffer viewer! You can view the front and back
    buffers, plus depth, stencil and auxiliary buffers as well as
    framebuffer objects. There is also better support for 3D textures in
    the texture viewer, and assorted bug-fixes.
    </p>
    <p>
    If you downloaded 0.0.20060827, you should upgrade to 0.0.20060828, as
    it fixes viewing of render-to-texture framebuffers.
    </p>
    <h2>Texture viewer improvements (13/08/2006)</h2>
    <p>
    This release makes some improvements to the texture viewer - in
    particular, shadow textures will be displayed in grey-scale rather than
    crashing, and luminance textures will appear in grey-scale rather than
    red-scale. Refer to the ChangeLog for miscellaneous other bug-fixes and
    improvements.
    </p>
    <h2>More bugfixes (29/05/2006)</h2>
    <p>
    It turns out that the shutdown code was not being called. This meant
    that the log would not be closed, videos would perhaps not be finalised,
    and the showextensions filter-set was useless. Furthermore, the
    break-on-error behaviour in the debuggers was broken, causing the
    application to continue running after an error.
    </p>
    <p>
    These bugs are fixed in the just-released 0.0.20060528.
    </p>
    <h2>Major bug in 0.0.20060416 (29/04/2006)</h2>
    <p>
    The previous release (0.0.20060416) added support for GCC 4.1, but in
    doing so it introduced a bug in the program that generates huge amounts
    of the code that goes into bugle. The consequences are difficult to
    predict, but one user reported crashes when using the <em>trace</em>
    filterset with <code>glMultMatrixf</code> and similar functions.
    </p>
    <p>
    All users of 0.0.20060429 should upgrade.
    </p>
    <h2>Camera filterset (16/04/2006)</h2>
    <p>
    The new 0.0.20060416 release adds a camera filterset, which allows the user to take
    over the camera. It won't work with advanced rendering techniques (such
    as any kind of post-processing), but is a handy way to test LOD and
    visibility culling.
    </p>
    <h2>Build fixes for Debian users (06/03/2006)</h2>
    <p>
    James Lyon has submitted a small patch needed to get BuGLe to compile on
    Debian testing. I've incorporated it into the new release, 0.0.20060306.
    Note that this release contains only build fixes and if you were able
    to compile 0.0.20060224 then there is no reason to use this release.
    </p>
    <h2>gl2ps integrated as a filter-set (24/02/2006)</h2>
    <p>
    Today's release incorporates <a href="http://www.geuz.org/gl2ps/">gl2ps</a>
    into a filter-set, allowing you to capture your scene as Postscript, EPS,
    PDF or SVG. The gl2ps mechanism itself has some limitations, and hence
    will not work with advanced applications that use per-pixel operations,
    but it does work with glxgears (if your browser supports SVG, click
    <a href="images/glxgears.svg" type="image/svg+xml">here</a> to see it).
    </p>
    <p>
    The snapshots are made possible by another new feature, keyboard
    interception. The screenshot filter-set also takes a key to trigger the
    screenshot, and some filter-sets (such as wireframe and video capture)
    can be enabled or disabled on the fly by keypress.
    </p>
    <p>
    As usual, there are also some miscellaneous bugfixes (see the ChangeLog
    for details), this time mostly affecting GTK+ 2.8.
    </p>
    <h2>Improved texture viewer and ATI driver support (1/1/2006)</h2>
    <p>
    I've worked on the texture viewer in the debugger and made it a lot 
    better. It is not yet complete (in particular 3D textures cannot be
    viewed, and luminance textures show up red), but should be an improvement
    on the previous release.
    </p>
    <p>
    I'd be interested to hear what features people would be interested in
    seeing in the texture viewer, and in fact gldb in general.
    </p>
    <p>I also spent a few days over Christmas with only my laptop, which has
    an ATI graphics card. I found some surprising bugs in the ATI driver
    (for example, during <tt>glDrawArrays</tt> the colour set with
    <tt>glColor3f</tt> is simply ignored), but also some bugs in my own
    code that only showed up with the ATI drivers. So those of you with
    ATI cards should now get a much better experience from BuGLe.
    </p>
    <h2>New release (0.0.20051112) adds preliminary GUI debugger (12/11/2005)</h2>
    <p>
    The promised GUI debugger is now ready in a preliminary form (meaning that
    lots of things aren't supported yet, but it shouldn't break). There are also
    several new extensions supported (see the changelog).
    </p>
    <h2>Patch against 0.0.20050512 (18/5/2005)</h2>
    <p>
    Luca Cappa has been very helpful pointing out bugs. <a href="downloads/bugle-0.0.20050512-patch-luca1">This patch</a>
    should fix a number of compile-time problems. After applying it, be sure to
    rerun <tt>configure</tt> and do a <tt>make clean</tt>.
    </p>
    <h2>New release fixes build error (12/5/2005)</h2>
    <p>
    I've just released a new version <a href="http://prdownloads.sourceforge.net/bugle/bugle-0.0.20050512.tar.bz2?download">(0.0.20050512)</a> that fixes the build problem listed in the previous post. It also fixes a few bugs and adds some new extensions to the state dump in gldb, so it is worth installing even if the previous release compiled for you.
    </p>
    <h2>Read this if you have a build error (11/5/2005)</h2>
    <p>Several people have emailed me to ask about build errors like the following:
    </p>
<pre>
filters/stats.c: In function `initialise_stats':                                
filters/stats.c:408: error: `GROUP_glBeginQueryARB' undeclared (first           
use in this function)                                                           
filters/stats.c:408: error: (Each undeclared identifier is reported only        
once                                                                            
filters/stats.c:408: error: for each function it appears in.)                   
filters/stats.c:408: error: `stats_fragments' undeclared (first use in          
this function)                                                                  
filters/stats.c:409: error: `GROUP_glEndQueryARB' undeclared (first use         
in this function)                                                               
</pre>
    <p>
    The short answer is yes, I messed up. I'll make a bugfix release soon, but in
    meantime here are two workarounds you can try.
    </p>
    <ol>
        <li>Update your <tt>glext.h</tt>. You can download the latest one from SGI
        <a href="http://oss.sgi.com/projects/ogl-sample/ABI/glext.h">here</a>. The file normally lives in /usr/include/GL/glext.h, although your OS may do something different. If you're not the sysadmin on your machine, you can put the downloaded file in a <tt>GL</tt> subdirectory of the bugle tree. Upgrading <tt>glext.h</tt>
        is a good idea anyway, since it will enable extra features of bugle.</li>
        <li>Repair the code. Either just remove the two offending lines, or else
        surround the enclosing <tt>if</tt> statement with the lines
        <kbd>#ifdef GL_ARB_occlusion_query</kbd> and <kbd>#endif</kbd></li>
    </ol>
    <h2>GUI debugger in the works (28/03/2005)</h2>
    <p>If you're wondering what's been happening since the last release, here's the answer: I've been working on a GUI version of the debugger. A big advantage over the text based debugger will be a texture viewer. There may also be a viewer for buffers (e.g. depth buffer, stencil buffer), but that might take a bit longer. You can see what the debugger looks like so far in <a href="gldb-gui.png">this</a> screenshot.
    <h2>Email address fixed (23/02/2005)</h2>
    <p>The email address that my sourceforge address forwards to disappeared
    several months ago, and I forgot to update the forward. My apologies to
    anyone who has tried to reach me. Please try again now - it should now go
    to the right address (or wait until 24/02/2005, since Sourceforge says
    it may take up to 24 hours to effect the change). A general reminder:
    bugs, support requests etc. mail to
    bmerry &lt;at] users.sourceforge (dot) net.</p>
    <h2>Version 0.0.20050221 released (21/02/2005)</h2>
    <p>
    After three months of development, the shiny new version is ready. The state
    manager has been completely rewritten, and now works much better. 
    It also supports very nearly all of the OpenGL 2.0 state. You can download it from the Sourceforge <a href="http://www.sourceforge.net/projects/bugle/">project page</a>.
    </p>

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
