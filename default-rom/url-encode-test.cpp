#include "url-encode.h"
#include "stdio.h"

int main(int argc, char** argv) {
    const char * encoded = "hello=world&test=a%20b&bool&foo%20bar=sth";
    UrlDecode dec(encoded);
    printf("hello=%s\n", dec.getKey("hello"));
    printf("test=%s\n", dec.getKey("test"));
    printf("bool=%s\n", dec.getKey("bool"));
    printf("foo bar=%s\n", dec.getKey("foo bar"));
    printf("non ex=%s\n", dec.getKey("non ex"));

    const char * encoded_wrong = "hello=world&&te&&=&st==a%20b&b&&&o\nol&fo&test=lala&o%20bar=sth&f%2h=%3";
    UrlDecode dec_wrong(encoded_wrong);
    printf("wrong: hello=%s\n", dec_wrong.getKey("hello"));
    printf("wrong: test=%s\n", dec_wrong.getKey("test"));
    printf("wrong: non ex=%s\n", dec_wrong.getKey("non ex"));

    return 0;
}
