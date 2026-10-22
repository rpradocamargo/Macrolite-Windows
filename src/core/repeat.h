// Repeat-count validation and the "Repeat [n] [ ] Unlimited" model.
#pragma once
#include <string>

namespace macrolite {

constexpr int kMinRepeatCount = 1;
constexpr int kMaxRepeatCount = 100000;

struct RepeatParseResult {
  bool ok = false;
  int value = 0;
  std::string error;
};

// Accepts optional surrounding whitespace and digits only. Range 1..kMaxRepeatCount.
RepeatParseResult parseRepeatCount(const std::string& text);

class RepeatSettings {
 public:
  int count() const { return count_; }
  bool unlimited() const { return unlimited_; }
  std::string countText() const { return std::to_string(count_); }

  // Stores the value only if it is valid. Returns whether it was valid.
  bool setCountText(const std::string& text);
  void setUnlimited(bool unlimited) { unlimited_ = unlimited; }

  // Number of repetitions to hand to the player; 0 means "until stopped".
  int playbackRepeats() const { return unlimited_ ? 0 : count_; }

 private:
  int count_ = 1;  // last valid value, preserved while "Unlimited" is checked
  bool unlimited_ = false;
};

}  // namespace macrolite
