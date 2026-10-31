#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>

#define BASE64_OKAY 0
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

bool file_write(const char* file_name, const u8* bytes, size_t count) {
    FILE* file = fopen(file_name, "wb");
    
    if (file == NULL) {
        fprintf(stderr, "Could not write to file '%s'.", file_name);
        return false;
    }

    fwrite(bytes, sizeof(bytes[0]), count, file);
    fclose(file);

    return true;
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

base64_t* base64_encode(const u8* _bytes, size_t bytes_length, size_t* base64_length) {
    size_t padding_count = (3 - bytes_length % 3) % 3;

    u8* bytes = calloc(bytes_length, sizeof(bytes[0]));
    memcpy(bytes, _bytes, bytes_length * sizeof(_bytes[0]));

    bytes_length += padding_count;

    base64_t* base64 = malloc(bytes_length * 4 / 3);
    size_t base64_i = 0;

    for (size_t bytes_i = 0; bytes_i < bytes_length; bytes_i += 3) {
        base64_encode_triplet(bytes + bytes_i, base64 + base64_i);
        base64_i += 4;
    }

    free(bytes);

    for (size_t i = 0; i < padding_count; i++) {
        base64[base64_i - i - 1] = '=';
    }

    *base64_length = base64_i;
    return base64;
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

    return BASE64_ERROR;

    #undef TRY
}

// Returns 0 if everything goes well
// Returns index + 1 if something goes wrong so you know where it happened
int base64_decode_quartet(base64_t* base64_quartet, u8* bytes_triplet) {
    #define TRY(index) \
    u8 value ## index = base64_digit_get_value(base64_quartet[index]); \
    if (value ## index == BASE64_ERROR) return index + 1;

    TRY(0);
    TRY(1);
    TRY(2);
    TRY(3);

    bytes_triplet[0] = (value0 << 2) | (value1 >> 4);
    bytes_triplet[1] = (value1 << 4) | (value2 >> 2);
    bytes_triplet[2] = (value2 << 6) | (value3 >> 0);

    return BASE64_OKAY;

    #undef TRY
}

u8* base64_decode(base64_t* base64, size_t base64_length, size_t* bytes_length) {
    if (base64_length % 4 != 0) {
        fprintf(stderr, "Invalid base64 string: length must be a multiple of 4. This might be fixed by adding one or two '=' characters at the end.\n");
        return NULL;
    }

    size_t padding_count = 0;

    // i could become >= base64_length due to overflow if every single character is '=' or if there are 0 characters
    for (size_t i = base64_length - 1; padding_count < 2 && i < base64_length && base64[i] == '='; i--) {
        padding_count++;
        base64[i] = 'A';
    }

    u8* bytes = malloc(base64_length * 3 / 4);
    size_t bytes_i = 0;

    for (size_t base64_i = 0; base64_i < base64_length; base64_i += 4) {
        int out = base64_decode_quartet(base64 + base64_i, bytes + bytes_i);

        if (out != 0) {
            int error_relative_index = out - 1;
            int error_absolute_index = base64_i + error_relative_index;
            base64_t error_digit = base64[error_absolute_index];

            if (error_digit == '=') {
                fprintf(
                    stderr,
                    "Invalid base64 string: padding character '%c' is only allowed at the end, a maximum of two times in a row, but was found at position %d (starting from 0)).\n",
                    error_digit, error_absolute_index
                );
            } else {
                fprintf(stderr, "Invalid base64 string: invalid base64 digit '%c' at position %d (starting from 0).\n", error_digit, error_absolute_index);
            }

            free(bytes);
            return NULL;
        }

        bytes_i += 3;
    }

    bytes_i -= padding_count; // Remove the padding null bytes

    *bytes_length = bytes_i;
    return bytes;
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

    size_t length;
    u8* src = file_read(src_file, &length);

    if (src == NULL) {
        printf("Could not read file '%s'.\n", src_file);
        return -1;
    }

    u8* dst;

    if (strcmp(cmd, "encode") == 0) {
        dst = base64_encode(src, length, &length);
    } else {
        dst = base64_decode(src, length, &length);

        if (dst == NULL) {
            free(src);
            return -1;
        }
    }

    int out = 0;

    if (dst_file == NULL) {
        // This is used instead of printf in case dst contains null characters
        // I don't want everything after to disappear
        fwrite(dst, sizeof(dst[0]), length, stdout);
        goto Success;
    }

    if (file_write(dst_file, dst, length)) {
        printf("Data was successfully written to file '%s'.\n", dst_file);
        goto Success;
    }

    out = -1;

Success:
    free(dst);
    free(src);

    return out;
}
