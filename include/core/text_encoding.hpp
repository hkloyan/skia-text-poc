#pragma once

#include <string>

namespace core {
namespace TextEncoding {

// Convert UTF-8 string to UTF-16
std::u16string toUtf16(const std::string& utf8);

// Convert UTF-16 string to UTF-8
std::string toUtf8(const std::u16string& utf16);

// Convert UTF-16 buffer to UTF-8
std::string toUtf8(const char16_t* utf16, size_t len);

} // namespace TextEncoding
} // namespace core
