#ifndef URLENCODE_H
#define URLENCODE_H

class UrlDecode {
public:
    UrlDecode(const char * url_encoded);
    char * getKey(const char * key);
private:
    char * url_encoded;
};

#endif //URLENCODE_H
