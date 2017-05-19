// vim: noai:ts=2:sw=2
#ifndef URLENCODE_H
#define URLENCODE_H
#include <Stream.h>

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

void urlEncodeWriteKeyValue(const char * key, const char * value, Stream & stream); 
#endif //URLENCODE_H

