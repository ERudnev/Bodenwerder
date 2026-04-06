#include "_common.h"

#include <base/containers/ImmutableUnorderedMap.h>
#include <base/containers/ImmutableUnorderedSet.h>

#include <algorithm>
#include <format>
#include <vector>

namespace tests {
    namespace {
        constexpr int sample_count = 20'000;
        constexpr int timing_rounds = 7;
        constexpr int find_repeat_count = 8;

        std::vector<int> make_keys() {
            std::vector<int> keys;
            keys.reserve(sample_count);

            for (int i = 0; i < sample_count; ++i) {
                keys.push_back(i * 104'729 + 17);
            }

            return keys;
        }

        int median_us(std::vector<int> samples) {
            std::sort(samples.begin(), samples.end());
            return samples[samples.size() / 2];
        }

        template<typename Fn>
        int measure_us(const char* name, Fn&& fn) {
            ::testing::scoped_timer timer(name);
            fn();
            const int elapsed_us = timer.elapsed_us();
            (void)timer.finish();
            return elapsed_us;
        }

        void report_phase(const char* name, const std::vector<int>& samples, int operations_per_round) {
            const auto [min_it, max_it] = std::minmax_element(samples.begin(), samples.end());
            const int median = median_us(samples);
            const double median_per_op_us = static_cast<double>(median) / static_cast<double>(operations_per_round);

            base::message(std::format(
                "{} median={:.3f} ms min={:.3f} ms max={:.3f} ms ({:.3f} us/op)",
                name,
                static_cast<double>(median) / 1'000.0,
                static_cast<double>(*min_it) / 1'000.0,
                static_cast<double>(*max_it) / 1'000.0,
                median_per_op_us
            ));
        }
    }

    void immutable_containers_perf() {
        const auto keys = make_keys();

        {
            {
                base::ImmutableUnorderedMap<int, int> warmup_map;
                for (int key : keys) {
                    warmup_map = warmup_map.insert(key, key + 1);
                }

                for (int repeat = 0; repeat < find_repeat_count; ++repeat) {
                    for (int key : keys) {
                        const int* value = warmup_map.find(key);
                        EXPECT_TRUE(value != nullptr) << "missing inserted key";
                    }
                }

                for (int i = 0; i < sample_count; i += 2) {
                    warmup_map = warmup_map.erase(keys[i]);
                }
            }

            std::vector<int> insert_samples;
            std::vector<int> find_samples;
            std::vector<int> erase_samples;
            insert_samples.reserve(timing_rounds);
            find_samples.reserve(timing_rounds);
            erase_samples.reserve(timing_rounds);

            for (int round = 0; round < timing_rounds; ++round) {
                base::ImmutableUnorderedMap<int, int> map;

                insert_samples.push_back(measure_us("immutable_map.insert", [&] {
                    for (int key : keys) {
                        map = map.insert(key, key + 1);
                    }
                }));

                EXPECT_EQ(map.size(), static_cast<std::size_t>(sample_count));

                long long checksum = 0;
                find_samples.push_back(measure_us("immutable_map.find", [&] {
                    for (int repeat = 0; repeat < find_repeat_count; ++repeat) {
                        for (int key : keys) {
                            const int* value = map.find(key);
                            EXPECT_TRUE(value != nullptr) << "missing inserted key";
                            checksum += *value;
                        }
                    }
                }));

                EXPECT_TRUE(checksum > 0);

                erase_samples.push_back(measure_us("immutable_map.erase_half", [&] {
                    for (int i = 0; i < sample_count; i += 2) {
                        map = map.erase(keys[i]);
                    }
                }));

                EXPECT_EQ(map.size(), static_cast<std::size_t>(sample_count / 2));
                for (int i = 0; i < sample_count; ++i) {
                    const bool should_exist = (i % 2) != 0;
                    EXPECT_EQ(map.contains(keys[i]), should_exist);
                }
            }

            report_phase("immutable_map.insert", insert_samples, sample_count);
            report_phase("immutable_map.find", find_samples, sample_count * find_repeat_count);
            report_phase("immutable_map.erase_half", erase_samples, sample_count / 2);
        }

        {
            {
                base::ImmutableUnorderedSet<int> warmup_set;
                for (int key : keys) {
                    warmup_set = warmup_set.insert(key);
                }

                for (int repeat = 0; repeat < find_repeat_count; ++repeat) {
                    for (int key : keys) {
                        const int* value = warmup_set.find(key);
                        EXPECT_TRUE(value != nullptr) << "missing inserted value";
                    }
                }

                for (int i = 0; i < sample_count; i += 2) {
                    warmup_set = warmup_set.erase(keys[i]);
                }
            }

            std::vector<int> insert_samples;
            std::vector<int> find_samples;
            std::vector<int> erase_samples;
            insert_samples.reserve(timing_rounds);
            find_samples.reserve(timing_rounds);
            erase_samples.reserve(timing_rounds);

            for (int round = 0; round < timing_rounds; ++round) {
                base::ImmutableUnorderedSet<int> set;

                insert_samples.push_back(measure_us("immutable_set.insert", [&] {
                    for (int key : keys) {
                        set = set.insert(key);
                    }
                }));

                EXPECT_EQ(set.size(), static_cast<std::size_t>(sample_count));

                int found_count = 0;
                find_samples.push_back(measure_us("immutable_set.find", [&] {
                    for (int repeat = 0; repeat < find_repeat_count; ++repeat) {
                        for (int key : keys) {
                            const int* value = set.find(key);
                            EXPECT_TRUE(value != nullptr) << "missing inserted value";
                            found_count += (*value == key) ? 1 : 0;
                        }
                    }
                }));

                EXPECT_EQ(found_count, sample_count * find_repeat_count);

                erase_samples.push_back(measure_us("immutable_set.erase_half", [&] {
                    for (int i = 0; i < sample_count; i += 2) {
                        set = set.erase(keys[i]);
                    }
                }));

                EXPECT_EQ(set.size(), static_cast<std::size_t>(sample_count / 2));
                for (int i = 0; i < sample_count; ++i) {
                    const bool should_exist = (i % 2) != 0;
                    EXPECT_EQ(set.contains(keys[i]), should_exist);
                }
            }

            report_phase("immutable_set.insert", insert_samples, sample_count);
            report_phase("immutable_set.find", find_samples, sample_count * find_repeat_count);
            report_phase("immutable_set.erase_half", erase_samples, sample_count / 2);
        }
    }
}
