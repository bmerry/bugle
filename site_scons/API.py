class Feature(dict):
    '''
    Holds list of headers, source files etc for a single aspect of the
    API featureset.
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
        bugleutils_sources = [])

class API:
    '''
    An API variant combination, built out of features
    '''
    def __init__(self, name, *features):
        self.name = name
        self.features = reduce(lambda x, y: x + y, features, _default_feature)
        for key, value in self.features.iteritems():
            self.__dict__[key] = value
