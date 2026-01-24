#pragma once

#include <string>

namespace testing {
    struct CountedItem {
        struct Pool {
            size_t count() const { return count_; }

        private:
            friend struct CountedItem;
            void add() { ++count_; }
            void remove() { --count_; }
            size_t count_ = 0;
        };

        explicit CountedItem(Pool& pool) : pool_(&pool) { pool_->add(); }
        CountedItem(Pool& pool, const std::string& value) : pool_(&pool), some_value(value) { pool_->add(); }
        ~CountedItem() { pool_->remove(); }
        CountedItem(const CountedItem& other) : pool_(other.pool_), some_value(other.some_value) { pool_->add(); }
        CountedItem(CountedItem&& other) noexcept : pool_(other.pool_), some_value(std::move(other.some_value)) { pool_->add(); }

        CountedItem& operator=(const CountedItem& other) {
            if (this != &other) {
                pool_->remove();
                pool_ = other.pool_;
                some_value = other.some_value;
                pool_->add();
            }
            return *this;
        }

        CountedItem& operator=(CountedItem&& other) noexcept {
            if (this != &other) {
                pool_->remove();
                pool_ = other.pool_;
                some_value = std::move(other.some_value);
                pool_->add();
            }
            return *this;
        }

        std::string some_value = "default_value";

    private:
        Pool* pool_;
    };
}
