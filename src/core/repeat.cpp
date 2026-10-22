#include "core/repeat.h"

#include <cctype>

namespace macrolite {

RepeatParseResult parseRepeatCount(const std::string& text) {
  RepeatParseResult r;
  const std::string range = "Enter a whole number from " + std::to_string(kMinRepeatCount) +
                            " to " + std::to_string(kMaxRepeatCount) + ".";
  size_t b = 0, e = text.size();
  while (b < e && std::isspace(static_cast<unsigned char>(text[b]))) ++b;
  while (e > b && std::isspace(static_cast<unsigned char>(text[e - 1]))) --e;
  if (b == e) {
    r.error = "Repeat count is empty. " + range;
    return r;
  }
  long long value = 0;
  for (size_t i = b; i < e; ++i) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    if (!std::isdigit(c)) {
      r.error = "Repeat count must contain digits only. " + range;
      return r;
    }
    value = value * 10 + (c - '0');
    if (value > kMaxRepeatCount) {  // early exit also prevents overflow
      r.error = "Repeat count is too large. " + range;
      return r;
    }
  }
  if (value < kMinRepeatCount) {
    r.error = "Repeat count must be at least 1. " + range;
    return r;
  }
  r.ok = true;
  r.value = static_cast<int>(value);
  return r;
}

bool RepeatSettings::setCountText(const std::string& text) {
  RepeatParseResult r = parseRepeatCount(text);
  if (!r.ok) return false;
  count_ = r.value;
  return true;
}

}  // namespace macrolite
