#include "core/text_encoding.hpp"
#include "modules/skunicode/include/SkUnicode.h"

namespace core {
namespace TextEncoding {

std::u16string toUtf16(const std::string& utf8) {
    return SkUnicode::convertUtf8ToUtf16(utf8.c_str(), static_cast<int>(utf8.size()));
}

std::string toUtf8(const std::u16string& utf16) {
    SkString sk = SkUnicode::convertUtf16ToUtf8(utf16);
    return std::string(sk.c_str(), sk.size());
}

std::string toUtf8(const char16_t* utf16, size_t len) {
    SkString sk = SkUnicode::convertUtf16ToUtf8(std::u16string(utf16, len));
    return std::string(sk.c_str(), sk.size());
}

} // namespace TextEncoding
} // namespace core
