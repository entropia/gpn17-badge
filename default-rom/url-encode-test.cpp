#include "url-encode.h"
#include "stdio.h"

int main(int argc, char** argv) {
    const char * encoded = "hello=world&test=a%20b&bool&foo%20bar=sth";
    UrlDecode dec(encoded);
    char * hello_ptr = dec.getKey("hello");
    printf("hello=%s\n", hello_ptr);
    delete[] hello_ptr;
    char * test_ptr = dec.getKey("test");
    printf("test=%s\n", test_ptr);
    delete[] test_ptr;
    char * bool_ptr = dec.getKey("bool");
    printf("bool=%s\n", bool_ptr);
    delete[] bool_ptr;
    char * foobar_ptr = dec.getKey("foo bar");
    printf("foo bar=%s\n", foobar_ptr);
    delete[] foobar_ptr;
    char * nonex_ptr = dec.getKey("non ex");
    printf("non ex=%s\n", nonex_ptr);

    const char * encoded_wrong = "hello=world&&te&&=&st==a%20b&b&&&o\nol&fo&test=lala&o%20bar=sth&f%2h=%3";
    UrlDecode dec_wrong(encoded_wrong);
    char * whello_ptr = dec_wrong.getKey("hello");
    printf("wrong: hello=%s\n", whello_ptr);
    delete[] whello_ptr;
    char * wtest_ptr = dec_wrong.getKey("test");
    printf("wrong: test=%s\n", wtest_ptr);
    delete[] wtest_ptr;
    char * wnonex_ptr = dec_wrong.getKey("non ex");
    printf("wrong: non ex=%s\n", wnonex_ptr);
    delete[] wnonex_ptr;

    return 0;
}
