#include <iQSM/identifier.h>

#include <chrono>
#include <cstdint>
#include <random>
#include <string>

namespace iqsm::internal::id {

BaseType generate_unique() {
  // Goals:
  // - simplest possible
  // - fully thread-safe (no shared state here)
  // - "globally unique with very high probability", collisions tolerated but unlikely
  //
  // Strategy:
  // - take current system time (nanoseconds)
  // - mix in OS-provided randomness (std::random_device)
  // - apply a small stateless bit-mixer to spread entropy across bits

  const std::uint64_t t = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count());

  std::random_device rd;
  const std::uint64_t r =
      (static_cast<std::uint64_t>(rd()) << 32) ^ static_cast<std::uint64_t>(rd());

  std::uint64_t x = t ^ (r + 0x9e3779b97f4a7c15ull);

  // Stateless 64-bit mix (SplitMix64 finalizer).
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
  x = x ^ (x >> 31);

  return static_cast<BaseType>(x);
}

std::string info_hash(BaseType id) {
  // Pure, deterministic, no-state debug fingerprint of a 64-bit id.
  // Output format: "abc-1234" (3 letters + '-' + 4 digits) ~= 28 bits of information.
  // Collisions are expected to be possible and are acceptable for debugging.

  std::uint64_t x = static_cast<std::uint64_t>(id);

  // Stateless 64-bit mix (SplitMix64 finalizer) to decorrelate patterns.
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
  x = x ^ (x >> 31);

  const std::uint32_t letters_part = static_cast<std::uint32_t>(x % (26u * 26u * 26u));
  const std::uint32_t digits_part = static_cast<std::uint32_t>((x / (26u * 26u * 26u)) % 10000u);

  const char a = static_cast<char>('a' + (letters_part / (26u * 26u)) % 26u);
  const char b = static_cast<char>('a' + (letters_part / 26u) % 26u);
  const char c = static_cast<char>('a' + (letters_part) % 26u);

  std::string out;
  out.resize(8);
  out[0] = a;
  out[1] = b;
  out[2] = c;
  out[3] = '-';
  out[4] = static_cast<char>('0' + (digits_part / 1000u) % 10u);
  out[5] = static_cast<char>('0' + (digits_part / 100u) % 10u);
  out[6] = static_cast<char>('0' + (digits_part / 10u) % 10u);
  out[7] = static_cast<char>('0' + (digits_part) % 10u);
  return out;
}

}  // namespace iqsm::internal::id


