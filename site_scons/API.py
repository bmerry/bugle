class Feature(dict):
    '''
    Holds list of headers, source files etc for a single aspect of the
    API featureset.

    Useful keys are:
      - C{checks}: a list of functions taking one parameter, a C{SCons.Configure}
      - C{headers}: the header files to search for function prototypes
      - C{gltype}, C{glwin}, C{winsys}: strings identifying the feature
      - C{bugle_libs}: libraries against which to link libbugle
      - C{bugle_sources}: extra source files for libbugle
      - C{bugleutils_sources}: extra source files for libbugleutils
    '''

    def __init__(self, **kw):
        for key, value in kw.iteritems():
            self[key] = value

    def __add__(self, second):
        ans = Feature()
        for key, value in self.iteritems():
            ans[key] = value
        for key, value in second.iteritems():
            if key in self:
                ans[key] = ans[key] + value
            else:
                ans[key] = value
        return ans

_default_feature = Feature(
        headers = [],
        bugle_libs = [],
        bugle_sources = [],
        bugleutils_sources = [],
        checks = [])

class API:
    '''
    An API variant combination, built out of features
    '''
    def __init__(self, name, *features):
        self.name = name
        self.features = reduce(lambda x, y: x + y, features, _default_feature)
        for key, value in self.features.iteritems():
            self.__dict__[key] = value
