// vim: noai:ts=2:sw=2
#include "url-encode.h"
#include <string.h>

void writeEscaped(const char * c, Stream & stream) {
  while(*c) {
    if(*c == '=') {
        stream.write("%3d");
    } else if(*c == '&') {
        stream.write("%26");
    } else {
        stream.write(*c);
    }
    c++;
  }
}

void urlEncodeWriteKeyValue(const char * key, const char * value, Stream & stream){
  writeEscaped(key, stream);
  stream.write('=');
  writeEscaped(value, stream);
  stream.write('&');
} 


UrlDecode::UrlDecode(const char * url_encoded) {
    size_t len = strlen(url_encoded) + 1;
    this->url_encoded = new char[len];
    memcpy(this->url_encoded, url_encoded, len);
}

UrlDecode::~UrlDecode() {
    delete[] url_encoded;
}

inline bool parse_hex_char(unsigned char * result, char c) {
        if ('0' <= c && c <= '9') {
            *result = c - '0';
            return true;
        }
        if ('a' <= c && c <= 'f') {
            *result = 10 + c - 'a';
            return true;
        }
        if ('A' <= c && c <= 'F') {
            *result = 10 + c - 'A';
            return true;
        }
}

bool hex_to_ascii(char * result, char charH, char charL) {
    unsigned char digitL;
    unsigned char digitH;
    if (!parse_hex_char(&digitL, charL)) {
        return false;
    }
    if (!parse_hex_char(&digitH, charH)) {
        return false;
    }
    *result = digitH << 4 | digitL;
    return true;
}

constexpr bool is_key_terminate_char(char c) {
    return c == '\0' || c == '=' || c == '&';
}

bool compare_key_encoded_unencoded(const char * encoded, const char * unencoded, const char * * char_after_key) {
    while (true) {
        char encoded_char = encoded[0];
        char unencoded_char = unencoded[0];

        if (is_key_terminate_char(encoded_char) && unencoded_char == '\0') {
            *char_after_key = encoded;
            return true;
        }
        if (is_key_terminate_char(encoded_char) || unencoded_char == '\0') {
            return false;
        }

        if (encoded_char == '%') {
            encoded++;
            if (is_key_terminate_char(encoded[0])) {
                return false;
            }
            char digit1 = encoded[0];

            encoded++;
            if (is_key_terminate_char(encoded[0])) {
                return false;
            }
            char digit2 = encoded[0];

            if (!hex_to_ascii(&encoded_char, digit1, digit2)) {
                return false;
            }
        }

        if (encoded_char == '+') {
          encoded_char = ' ';
        }

        if (encoded_char != unencoded_char) {
            return false;
        }

        encoded++;
        unencoded++;
        
    }
}

bool decode_percent(const char * encoded, size_t encoded_len, char * decoded, size_t * decoded_len) {
    size_t i = 0;
    *decoded_len = 0;
    while (i < encoded_len) {
        if (encoded[0] == '%') {
            encoded++;
            i++;
            if (i >= encoded_len) {
                return false;
            }
            char digit1 = encoded[0];

            encoded++;
            i++;
            if (i >= encoded_len) {
                return false;
            }
            char digit2 = encoded[0];

            if (!hex_to_ascii(decoded, digit1, digit2)) {
                return false;
            }
        } else if (encoded[0] == '+') {
          decoded[0] = ' ';
        } else {
            decoded[0] = encoded[0];
        }
        decoded++;
        (*decoded_len)++;
        encoded++;
        i++;
    }
    return true;
}

char * UrlDecode::getKey(const char * key) {
    const char * encoded = url_encoded;
    while (true) {
        const char * char_after_key;
        if (compare_key_encoded_unencoded(encoded, key, &char_after_key)) {
            // Found key
            const char * end_of_val = char_after_key;
            while(end_of_val[0] != '\0' && end_of_val[0] != '&') {
                end_of_val++;
            }
            if (*char_after_key == '=') {
                char_after_key++;
            }
            size_t len = end_of_val - char_after_key;
            char * result = new char[len + 1];
            size_t res_len;
            decode_percent(char_after_key, len, result, &res_len);
            result[res_len] = '\0';
            return result;
        } else {
            // Skip &
            if (encoded[0] != '\0') {
                encoded++;
            }
            // Go to next key
            while (encoded[0] != '\0' && encoded[0] != '&') {
                encoded++;
            }
            if (encoded[0] == '\0') {
                // Hit end of string
                return nullptr;
            }
            encoded++;
        }

    }

}

