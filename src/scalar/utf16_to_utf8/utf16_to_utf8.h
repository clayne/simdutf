#ifndef SIMDUTF_UTF16_TO_UTF8_H
#define SIMDUTF_UTF16_TO_UTF8_H

namespace simdutf {
namespace scalar {
namespace {
namespace utf16_to_utf8 {

template <endianness big_endian>
inline size_t convert(const char16_t *buf, size_t len, char *utf8_output) {
  const uint16_t *data = reinterpret_cast<const uint16_t *>(buf);
  size_t pos = 0;
  char *start{utf8_output};
  while (pos < len) {
    // try to convert the next block of 8 bytes
    if (pos + 4 <=
        len) { // if it is safe to read 8 more bytes, check that they are ascii
      uint64_t v;
      ::memcpy(&v, data + pos, sizeof(uint64_t));
      if (!match_system(big_endian)) {
        v = (v >> 8) | (v << (64 - 8));
      }
      if ((v & 0xFF80FF80FF80FF80) == 0) {
        size_t final_pos = pos + 4;
        while (pos < final_pos) {
          *utf8_output++ = !match_system(big_endian)
                               ? char(u16_swap_bytes(buf[pos]))
                               : char(buf[pos]);
          pos++;
        }
        continue;
      }
    }
    uint16_t word =
        !match_system(big_endian) ? u16_swap_bytes(data[pos]) : data[pos];
    if ((word & 0xFF80) == 0) {
      // will generate one UTF-8 bytes
      *utf8_output++ = char(word);
      pos++;
    } else if ((word & 0xF800) == 0) {
      // will generate two UTF-8 bytes
      // we have 0b110XXXXX 0b10XXXXXX
      *utf8_output++ = char((word >> 6) | 0b11000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;
    } else if ((word & 0xF800) != 0xD800) {
      // will generate three UTF-8 bytes
      // we have 0b1110XXXX 0b10XXXXXX 0b10XXXXXX
      *utf8_output++ = char((word >> 12) | 0b11100000);
      *utf8_output++ = char(((word >> 6) & 0b111111) | 0b10000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;
    } else {
      // must be a surrogate pair
      if (pos + 1 >= len) {
        return 0;
      }
      uint16_t diff = uint16_t(word - 0xD800);
      if (diff > 0x3FF) {
        return 0;
      }
      uint16_t next_word = !match_system(big_endian)
                               ? u16_swap_bytes(data[pos + 1])
                               : data[pos + 1];
      uint16_t diff2 = uint16_t(next_word - 0xDC00);
      if (diff2 > 0x3FF) {
        return 0;
      }
      uint32_t value = (diff << 10) + diff2 + 0x10000;
      // will generate four UTF-8 bytes
      // we have 0b11110XXX 0b10XXXXXX 0b10XXXXXX 0b10XXXXXX
      *utf8_output++ = char((value >> 18) | 0b11110000);
      *utf8_output++ = char(((value >> 12) & 0b111111) | 0b10000000);
      *utf8_output++ = char(((value >> 6) & 0b111111) | 0b10000000);
      *utf8_output++ = char((value & 0b111111) | 0b10000000);
      pos += 2;
    }
  }
  return utf8_output - start;
}

template <endianness big_endian, bool check_output = false>
inline full_result convert_with_errors(const char16_t *buf, size_t len,
                                       char *utf8_output, size_t utf8_len = 0) {
  const uint16_t *data = reinterpret_cast<const uint16_t *>(buf);
  if (check_output && utf8_len == 0) {
    return full_result(error_code::OUTPUT_BUFFER_TOO_SMALL, 0, 0);
  }

  size_t pos = 0;
  char *start{utf8_output};
  char *end{utf8_output + utf8_len};

  while (pos < len) {
    // try to convert the next block of 8 bytes
    if (pos + 4 <=
        len) { // if it is safe to read 8 more bytes, check that they are ascii
      uint64_t v;
      ::memcpy(&v, data + pos, sizeof(uint64_t));
      if (!match_system(big_endian))
        v = (v >> 8) | (v << (64 - 8));
      if ((v & 0xFF80FF80FF80FF80) == 0) {
        size_t final_pos = pos + 4;
        while (pos < final_pos) {
          *utf8_output++ = !match_system(big_endian)
                               ? char(u16_swap_bytes(buf[pos]))
                               : char(buf[pos]);
          pos++;
          if (check_output && size_t(end - utf8_output) == 0) {
            return full_result(error_code::OUTPUT_BUFFER_TOO_SMALL, pos,
                               utf8_output - start);
          }
        }
        continue;
      }
    }
    uint16_t word =
        !match_system(big_endian) ? u16_swap_bytes(data[pos]) : data[pos];
    if ((word & 0xFF80) == 0) {
      // will generate one UTF-8 bytes
      *utf8_output++ = char(word);
      pos++;
      if (check_output && size_t(end - utf8_output) == 0) {
        return full_result(error_code::OUTPUT_BUFFER_TOO_SMALL, pos,
                           utf8_output - start);
      }
    } else if ((word & 0xF800) == 0) {
      // will generate two UTF-8 bytes
      // we have 0b110XXXXX 0b10XXXXXX
      if (check_output && size_t(end - utf8_output) < 2) {
        return full_result(error_code::OUTPUT_BUFFER_TOO_SMALL, pos,
                           utf8_output - start);
      }
      *utf8_output++ = char((word >> 6) | 0b11000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;

    } else if ((word & 0xF800) != 0xD800) {
      // will generate three UTF-8 bytes
      // we have 0b1110XXXX 0b10XXXXXX 0b10XXXXXX
      if (check_output && size_t(end - utf8_output) < 3) {
        return full_result(error_code::OUTPUT_BUFFER_TOO_SMALL, pos,
                           utf8_output - start);
      }
      *utf8_output++ = char((word >> 12) | 0b11100000);
      *utf8_output++ = char(((word >> 6) & 0b111111) | 0b10000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;
    } else {

      if (check_output && size_t(end - utf8_output) < 4) {
        return full_result(error_code::OUTPUT_BUFFER_TOO_SMALL, pos,
                           utf8_output - start);
      }
      // must be a surrogate pair
      if (pos + 1 >= len) {
        return full_result(error_code::SURROGATE, pos, utf8_output - start);
      }
      uint16_t diff = uint16_t(word - 0xD800);
      if (diff > 0x3FF) {
        return full_result(error_code::SURROGATE, pos, utf8_output - start);
      }
      uint16_t next_word = !match_system(big_endian)
                               ? u16_swap_bytes(data[pos + 1])
                               : data[pos + 1];
      uint16_t diff2 = uint16_t(next_word - 0xDC00);
      if (diff2 > 0x3FF) {
        return full_result(error_code::SURROGATE, pos, utf8_output - start);
      }
      uint32_t value = (diff << 10) + diff2 + 0x10000;
      // will generate four UTF-8 bytes
      // we have 0b11110XXX 0b10XXXXXX 0b10XXXXXX 0b10XXXXXX
      *utf8_output++ = char((value >> 18) | 0b11110000);
      *utf8_output++ = char(((value >> 12) & 0b111111) | 0b10000000);
      *utf8_output++ = char(((value >> 6) & 0b111111) | 0b10000000);
      *utf8_output++ = char((value & 0b111111) | 0b10000000);
      pos += 2;
    }
  }
  return full_result(error_code::SUCCESS, pos, utf8_output - start);
}

template <endianness big_endian>
inline result simple_convert_with_errors(const char16_t *buf, size_t len,
                                         char *utf8_output) {
  return convert_with_errors<big_endian, false>(buf, len, utf8_output, 0);
}

} // namespace utf16_to_utf8
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
