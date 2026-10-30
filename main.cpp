#include <iostream>
#include <fstream>
#include <sstream>

#include <print>
#include <algorithm>

#include <string>
#include <string_view>

#include <optional>
#include <expected>

using u8 = unsigned char;

std::optional<std::string> file_read(const std::string& file_path) {
    std::ifstream file(file_path, std::ios_base::binary);
    if (!file.good()) return std::nullopt;

    std::stringstream sstr;
    sstr << file.rdbuf();

    return sstr.str();
}

namespace base64 {
    using Result = std::expected<std::string, std::string>;

    using Base64Digit = char;

    using BytesTriplet = std::array<u8, 3>;
    using Base64Quartet = std::array<Base64Digit, 4>;

    constexpr std::array<std::pair<Base64Digit, Base64Digit>, 5> sections = {
        std::pair { 'A', 'Z' },
        std::pair { 'a', 'z' },
        std::pair { '0', '9' },
        std::pair { '+', '+' },
        std::pair { '/', '/' },
    };

    constexpr Base64Digit padding_character = '=';

    std::optional<Base64Digit> value_to_base64_digit(u8 value) {
        Base64Digit accumulated = 0;

        for (auto [first, last] : sections) {
            if (value <= last - first + accumulated) return first + value - accumulated;
            accumulated += last - first + 1;
        }

        return std::nullopt;
    }

    Base64Quartet triplet_to_quartet(const BytesTriplet& triplet) {
        Base64Quartet out = {
            (Base64Digit) (triplet[0] >> 2),
            (Base64Digit) (((triplet[0] & 0b11) << 4) | (triplet[1] >> 4)),
            (Base64Digit) (((triplet[1] & 0b1111) << 2) | (triplet[2] >> 6)),
            (Base64Digit) (((triplet[2] & 0b111111))),
        };

        for (Base64Digit& digit : out) {
            digit = *value_to_base64_digit(digit);
        }

        return out;
    }

    std::string encode(const std::string& _bytes) {
        std::string bytes = _bytes;
        std::size_t padding = 0;

        while (bytes.size() % 3 != 0) {
            padding++;
            bytes.push_back('\0');
        }

        std::string base64;
        base64.reserve(bytes.size() * 4 / 3);

        for (std::size_t i = 0; i < bytes.size(); i += 3) {
            BytesTriplet triplet;
            std::copy(bytes.begin() + i, bytes.begin() + i + triplet.size(), triplet.begin());

            for (Base64Digit digit : triplet_to_quartet(triplet)) {
                base64.push_back(digit);
            }
        }

        while (padding--) {
            base64[base64.size() - padding - 1] = padding_character;
        }

        return base64;
    }

    std::optional<u8> base64_digit_to_value(Base64Digit digit) {
        u8 accumulated = 0;

        for (auto [first, last] : sections) {
            if (digit >= first && digit <= last) return digit - first + accumulated;
            accumulated += last - first + 1;
        }

        return std::nullopt;
    }

    std::expected<BytesTriplet, std::size_t> quartet_to_triplet(const Base64Quartet& quartet) {
        Base64Quartet values;
        
        for (std::size_t i = 0; i < quartet.size(); i++) {
            auto value = base64_digit_to_value(quartet[i]);
            if (!value) return std::unexpected(i);
            values[i] = *value;
        }
        
        return BytesTriplet {
            (u8) ((values[0] << 2) | (values[1] >> 4)),
            (u8) ((values[1] << 4) | (values[2] >> 2)),
            (u8) ((values[2] << 6) | (values[3] >> 0)),
        };
    }

    Result decode(const std::string& _base64) {
        std::string base64 = _base64;

        if (base64.size() % 4 != 0) {
            return std::unexpected(std::format("length must be a multiple of 4, but was {}. This might be fixed by adding one or two '=' characters at the end", base64.size()));
        }

        std::size_t padding = 0;

        for (std::size_t _i = 0; _i < 2; _i++) {
            std::size_t i = base64.size() - _i - 1;

            if (base64[i] == padding_character) {
                padding++;
                base64[i] = *value_to_base64_digit(0);
            }
        }

        std::string bytes;
        bytes.reserve(base64.size() * 3 / 4);

        for (std::size_t i = 0; i < base64.size(); i += 4) {
            Base64Quartet quartet;
            std::copy(base64.begin() + i, base64.begin() + i + quartet.size(), quartet.begin());

            auto triplet = quartet_to_triplet(quartet);

            if (!triplet) {
                std::size_t error_quartet_index = triplet.error();
                std::size_t error_absolute_index = i + error_quartet_index;
                Base64Digit error_digit = quartet[error_quartet_index];

                if (error_digit == padding_character) {
                    return std::unexpected(std::format("padding character '{}' is only allowed at the end, a maximum of two times in a row, but was found at position {} (starting from 0))",
                        error_digit, error_absolute_index));
                } else {
                    return std::unexpected(std::format("invalid base64 digit '{}' at position {} (starting from 0)", error_digit, error_absolute_index));
                }
            }

            for (u8 byte : *triplet) {
                bytes.push_back(byte);
            }
        }

        while (padding--) {
            bytes.pop_back();
        }

        return bytes;
    }
}

int main(int argc, char** argv) {
    if ((argc != 3 && argc != 4) || (argv[1] != std::string_view("encode") && argv[1] != std::string_view("decode"))) {
        std::println("Usage:");
        std::println("base64 encode <src_file> <dst_file>?");
        std::println("base64 decode <src_file> <dst_file>?");
        return -1;
    }

    std::string src_file = argv[2];
    std::optional<std::string> dst_file_opt = argc == 4 ? std::optional(argv[3]) : std::nullopt;

    std::string src;

    if (auto src_opt = file_read(src_file)) {
        src = *src_opt;
    } else {
        std::println(std::cerr, "Could not read file '{}'.", src_file);
        return -1;
    }

    base64::Result dst_opt;

    if (argv[1] == std::string_view("encode")) {
        dst_opt = base64::encode(src);
    } else {
        dst_opt = base64::decode(src);
    }

    if (dst_opt) {
        std::string dst = std::move(*dst_opt);

        if (dst_file_opt) {
            std::string dst_file = std::move(*dst_file_opt);
            std::ofstream file(dst_file, std::ios_base::binary);

            if (file.good()) {
                std::print(file, "{}", dst);
                std::println("Data was successfully written to file '{}'.", dst_file);
            } else {
                std::println(std::cerr, "Could not write to file '{}'.", dst_file);
            }
        } else {
            std::println("{}", dst);
        }
    } else {
        std::println(std::cerr, "Invalid base64 string: {}.", dst_opt.error());
    }

    return 0;
}
