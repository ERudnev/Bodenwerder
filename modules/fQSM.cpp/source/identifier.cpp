#include <fQSM/identifier.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <random>
#include <string>

namespace fqsm::internal::id {

namespace {

std::uint64_t next_entropy_word() {
  thread_local std::mt19937_64 rng = [] {
    std::random_device rd;
    const auto mix = [](std::uint64_t x) {
      x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
      x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
      return x ^ (x >> 31);
    };

    std::array<std::uint32_t, 8> seed_data{};
    for (auto& word : seed_data) {
      word = rd();
    }

    const auto now = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
    seed_data[0] ^= static_cast<std::uint32_t>(now);
    seed_data[1] ^= static_cast<std::uint32_t>(now >> 32);

    std::seed_seq seed(seed_data.begin(), seed_data.end());
    std::mt19937_64 engine(seed);
    engine.discard(static_cast<unsigned long long>(mix(now) & 0xffu));
    return engine;
  }();

  return rng();
}

}  // namespace

BaseType generate_unique() {
  const std::uint64_t t = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count());

  const std::uint64_t r = next_entropy_word();

  std::uint64_t x = t ^ (r + 0x9e3779b97f4a7c15ull);

  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
  x = x ^ (x >> 31);

  return static_cast<BaseType>(x);
}

std::string info_hash(BaseType id) {
  std::uint64_t x = static_cast<std::uint64_t>(id);

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

}  // namespace fqsm::internal::id
