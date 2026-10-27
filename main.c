#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#define BASE64_ERROR 64

typedef unsigned char u8;
typedef unsigned char base64_t;

u8* file_read(const char* file_name, size_t* out_bytes_read) {
    FILE* file = fopen(file_name, "rb");
    if (file == NULL) return NULL;

    fseek(file, 0, SEEK_END);
    size_t length = ftell(file);
    fseek(file, 0, SEEK_SET);

    u8* buf = malloc(length + 1);
    buf[length] = '\0';

    *out_bytes_read = fread(buf, sizeof(buf[0]), length, file);
    fclose(file);

    return buf;
}

void file_write(const char* file_name, const u8* bytes, size_t count) {
    FILE* file = fopen(file_name, "wb");
    fwrite(bytes, sizeof(bytes[0]), count, file);
    fclose(file);
}

u8 base64_digit_get_value(base64_t digit) {
    #define TRY(smallest, biggest) \
    do { \
        if (digit >= smallest && digit <= biggest) { \
            return digit - smallest + accumulated; \
        } \
        accumulated += biggest - smallest + 1; \
    } while (0)

    u8 accumulated = 0;

    TRY('A', 'Z');
    TRY('a', 'z');
    TRY('0', '9');
    TRY('+', '+');
    TRY('/', '/');

    return digit == '=' ? 0 : BASE64_ERROR;

    #undef TRY
}

void base64_decode_quartet(base64_t* base64_quartet, u8* bytes_triplet) {
    u8 digit0 = base64_digit_get_value(base64_quartet[0]);
    u8 digit1 = base64_digit_get_value(base64_quartet[1]);
    u8 digit2 = base64_digit_get_value(base64_quartet[2]);
    u8 digit3 = base64_digit_get_value(base64_quartet[3]);

    bytes_triplet[0] = (digit0 << 2) | (digit1 >> 4);
    bytes_triplet[1] = (digit1 << 4) | (digit2 >> 2);
    bytes_triplet[2] = (digit2 << 6) | (digit3 >> 0);
}

void base64_decode(const char* src_file, const char* dst_file) {
    size_t base64_length;
    base64_t* base64 = file_read(src_file, &base64_length);

    if (base64 == NULL) {
        printf("Could not read file \"%s\".\n", src_file);
        exit(-1);
    }

    if (base64_length % 4 != 0) {
        printf("Invalid base64 string: length must be a multiple of 4. This might be fixed by adding some '=' characters at the end.\n");
        exit(-1);
    }

    size_t padding_count = 0;

    for (size_t i = base64_length - 1; base64[i] == '='; i--) {
        padding_count++;
    }

    u8* bytes = malloc(base64_length * 3 / 4);
    size_t bytes_i = 0;

    for (size_t base64_i = 0; base64_i < base64_length; base64_i += 4) {
        base64_decode_quartet(base64 + base64_i, bytes + bytes_i);
        bytes_i += 3;
    }

    bytes_i -= padding_count; // Remove the padding null bytes

    if (dst_file == NULL) {
        printf("%.*s\n", bytes_i, bytes);
    } else {
        file_write(dst_file, bytes, bytes_i);
    }

    free(base64);
    free(bytes);
}

base64_t base64_value_get_digit(u8 value) {
    #define TRY(smallest, biggest) \
    do { \
        if (value <= biggest - smallest) { \
            return value + smallest; \
        } \
        value -= biggest - smallest + 1; \
    } while (0)

    TRY('A', 'Z');
    TRY('a', 'z');
    TRY('0', '9');
    TRY('+', '+');
    TRY('/', '/');

    return BASE64_ERROR;

    #undef TRY
}

void base64_encode_triplet(u8* bytes_triplet, base64_t* base64_quartet) {
    base64_quartet[0] = base64_value_get_digit(bytes_triplet[0] >> 2);
    base64_quartet[1] = base64_value_get_digit(((bytes_triplet[0] & 0x3) << 4) | (bytes_triplet[1] >> 4));
    base64_quartet[2] = base64_value_get_digit(((bytes_triplet[1] & 0xF) << 2) | (bytes_triplet[2] >> 6));
    base64_quartet[3] = base64_value_get_digit(bytes_triplet[2] & 0x3F);
}

void base64_encode(const char* src_file, const char* dst_file) {
    size_t bytes_length;
    u8* bytes = file_read(src_file, &bytes_length);
    size_t padding_count = (3 - bytes_length % 3) % 3;

    if (padding_count > 0) {
        bytes = realloc(bytes, bytes_length + padding_count);

        for (size_t i = 0; i < padding_count; i++) {
            bytes[bytes_length++] = '\0';
        }
    }

    base64_t* base64 = malloc(bytes_length * 4 / 3);
    size_t base64_i = 0;

    for (size_t bytes_i = 0; bytes_i < bytes_length; bytes_i += 3) {
        base64_encode_triplet(bytes + bytes_i, base64 + base64_i);
        base64_i += 4;
    }

    for (size_t i = 0; i < padding_count; i++) {
        base64[base64_i - i - 1] = '=';
    }

    if (dst_file == NULL) {
        printf("%.*s\n", base64_i, base64);
    } else {
        file_write(dst_file, base64, base64_i);
    }
}

int main(int argc, char** argv) {
    if ((argc != 3 && argc != 4) || (strcmp(argv[1], "encode") != 0 && strcmp(argv[1], "decode") != 0)) {
        printf("Usage:\n");
        printf("base64 encode <src_file> <dst_file>?\n");
        printf("base64 decode <src_file> <dst_file>?\n");
        return -1;
    }

    const char* cmd = argv[1];
    const char* src_file = argv[2];
    const char* dst_file = argc == 4 ? argv[3] : NULL;

    if (strcmp(cmd, "encode") == 0) base64_encode(src_file, dst_file);
    else base64_decode(src_file, dst_file);

    return 0;
}
