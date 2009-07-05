from SCons import *
import xml.parsers.expat

def xinclude_scanner_function(node, env, path, arg = None):
    """
    Extracts XInclude lines from an XML file to use as dependencies
    """

    ret = []
    def start_handler(target, data):
        if target == 'http://www.w3.org/2001/XInclude$include' or target == 'http://www.w3.org/1999/XSL/Transform$include':
            if 'href' in data:
                ret.append(data['href'])

    p = xml.parsers.expat.ParserCreate(namespace_separator = '$')
    p.StartElementHandler = start_handler
    try:
        p.Parse(node.get_contents(), True)
    except xml.parsers.expat.ExpatError:
        # If the file is not well-formed XML, get an error during compilation
        # rather than during scanning
        pass
    return ret

def generate(env, **kw):
    xinclude_scanner = env.Scanner(
            function = xinclude_scanner_function,
            recursive = 1)
    docbook_builder = env.Builder(
            action = [
                Action.Action('$DOCBOOKLINTCOM', '$DOCBOOKLINTCOMSTR'),
                Action.Action('$DOCBOOKCOM', '$DOCBOOKCOMSTR')],
            source_scanner = xinclude_scanner,
            target_factory = env.File)
    env.Append(
            XSLTPROC = 'xsltproc',
            XMLLINT = 'xmllint',
            DOCBOOKCOM = '$XSLTPROC --xinclude $DOCBOOKFLAGS -o $TARGET $SOURCES',
            DOCBOOKLINTCOM = '$XMLLINT --noent --noout --xinclude --postvalid ${SOURCES[1]}',
            BUILDERS = {'DocBook': docbook_builder}
            )

def exists(env):
    return 1
