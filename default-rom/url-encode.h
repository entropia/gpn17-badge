#ifndef URLENCODE_H
#define URLENCODE_H

class UrlDecode {
public:
    UrlDecode(const char * url_encoded);
    ~UrlDecode();

    /// returns the value to a given key
    /// You have to delete[] the resulting char * yourself
    char * getKey(const char * key);

private:
    char * url_encoded;
};

#endif //URLENCODE_H
