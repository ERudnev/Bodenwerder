#pragma once
#include <memory>
#include <functional>
#include <utility>
#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <optional>
#include <vector>
#include <cstdint>

namespace base {

template<typename Key, typename T, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
class ImmutableUnorderedMap {
private:
    // Forward declaration
    struct Node;
    
    // Leaf node for storing actual key-value pairs
    struct Leaf {
        std::pair<const Key, T> value_;
        
        Leaf(const Key& key, const T& val) : value_(key, val) {}
        Leaf(Key&& key, T&& val) : value_(std::move(key), std::move(val)) {}
    };
    
    // Entry in HAMT node - can be either internal node or leaf
    struct Entry {
        enum Type { NODE, LEAF };
        Type type_;
        std::shared_ptr<Node> node_;
        std::shared_ptr<Leaf> leaf_;
        
        Entry(std::shared_ptr<Node> node) : type_(NODE), node_(node), leaf_(nullptr) {}
        Entry(std::shared_ptr<Leaf> leaf) : type_(LEAF), node_(nullptr), leaf_(leaf) {}
        
        // Copy constructor for structural sharing
        Entry(const Entry& other) 
            : type_(other.type_), node_(other.node_), leaf_(other.leaf_) {}
    };
    
    // HAMT node structure
    struct Node {
        uint32_t bitmap_;  // Bitmap indicating which positions are occupied (32-way branching)
        std::vector<Entry> children_;  // Array of child entries (nodes or leaves)
        
        Node() : bitmap_(0) {}
        
        // Copy constructor - shallow copy for structural sharing
        Node(const Node& other) 
            : bitmap_(other.bitmap_), children_(other.children_) {}
        
        // Move constructor
        Node(Node&& other) noexcept 
            : bitmap_(other.bitmap_), children_(std::move(other.children_)) {
            other.bitmap_ = 0;
        }
    };
    
    static constexpr int HASH_SHIFT = 5;  // 5 bits per level (32-way branching)
    static constexpr int HASH_MASK = 0x1F; // Mask for 5 bits
    
    std::shared_ptr<Node> root_;
    size_t size_;
    Hash hasher_;
    KeyEqual key_equal_;
    
    // Helper: count set bits (popcount)
    static int popcount(uint32_t x) {
#ifdef _MSC_VER
        return __popcnt(x);
#else
        return __builtin_popcount(x);
#endif
    }
    
    // Helper: get bit position in bitmap for given hash and level
    static uint32_t bitpos(size_t hash, int level) {
        return 1U << ((hash >> (level * HASH_SHIFT)) & HASH_MASK);
    }
    
    // Helper: get index in children array for given bit
    static int index(uint32_t bitmap, uint32_t bit) {
        return popcount(bitmap & (bit - 1));
    }
    
    // Find value in HAMT
    const T* find_impl(const std::shared_ptr<Node>& node, const Key& key, size_t hash, int level) const {
        if (!node) return nullptr;
        
        uint32_t bit = bitpos(hash, level);
        
        if (!(node->bitmap_ & bit)) {
            return nullptr;  // Bit not set, key doesn't exist
        }
        
        int idx = index(node->bitmap_, bit);
        auto& entry = node->children_[idx];
        
        if (entry.type_ == Entry::NODE) {
            // Recursive search in internal node
            return find_impl(entry.node_, key, hash, level + 1);
        } else {
            // Check leaf
            if (key_equal_(entry.leaf_->value_.first, key)) {
                return &entry.leaf_->value_.second;
            }
            // Hash collision - key not found
            return nullptr;
        }
    }
    
    // Set value in HAMT (returns new node, or same node if no change)
    std::shared_ptr<Node> set_impl(const std::shared_ptr<Node>& node, const Key& key, const T& value, size_t hash, int level) const {
        if (!node) {
            // Create new leaf node
            auto leaf = std::make_shared<Leaf>(key, value);
            auto new_node = std::make_shared<Node>();
            uint32_t bit = bitpos(hash, level);
            new_node->bitmap_ = bit;
            new_node->children_.emplace_back(leaf);
            return new_node;
        }
        
        uint32_t bit = bitpos(hash, level);
        
        if (node->bitmap_ & bit) {
            // Position occupied
            int idx = index(node->bitmap_, bit);
            auto& entry = node->children_[idx];
            
            if (entry.type_ == Entry::NODE) {
                // Recursive set
                auto new_child = set_impl(entry.node_, key, value, hash, level + 1);
                if (new_child == entry.node_) {
                    return node;  // No change
                }
                // Create new node with updated child
                auto new_node = std::make_shared<Node>(*node);
                new_node->children_[idx] = Entry(new_child);
                return new_node;
            } else {
                // Check if key matches
                if (key_equal_(entry.leaf_->value_.first, key)) {
                    // Update existing value
                    auto new_node = std::make_shared<Node>(*node);
                    auto new_leaf = std::make_shared<Leaf>(key, value);
                    new_node->children_[idx] = Entry(new_leaf);
                    return new_node;
                }
                
                // Hash collision - need to create internal node
                size_t existing_hash = hasher_(entry.leaf_->value_.first);
                auto new_node = std::make_shared<Node>(*node);
                new_node->children_[idx] = Entry(create_collision_node(
                    entry.leaf_, std::make_shared<Leaf>(key, value), 
                    existing_hash, hash, level + 1));
                return new_node;
            }
        } else {
            // Position free - insert leaf directly
            auto new_node = std::make_shared<Node>(*node);
            new_node->bitmap_ |= bit;
            int idx = index(node->bitmap_, bit);
            auto leaf = std::make_shared<Leaf>(key, value);
            new_node->children_.insert(new_node->children_.begin() + idx, Entry(leaf));
            return new_node;
        }
    }
    
    // Handle hash collision by creating internal node
    std::shared_ptr<Node> create_collision_node(
        const std::shared_ptr<Leaf>& existing_leaf, 
        const std::shared_ptr<Leaf>& new_leaf,
        size_t existing_hash,
        size_t new_hash,
        int level) const {
        
        auto node = std::make_shared<Node>();
        
        // Check if hashes still collide at this level
        uint32_t existing_bit = bitpos(existing_hash, level);
        uint32_t new_bit = bitpos(new_hash, level);
        
        if (existing_bit == new_bit) {
            // Still colliding - recurse deeper
            auto deeper_node = create_collision_node(existing_leaf, new_leaf, existing_hash, new_hash, level + 1);
            node->bitmap_ = existing_bit;
            node->children_.emplace_back(deeper_node);
        } else {
            // Hashes diverged - can place both
            node->bitmap_ = existing_bit | new_bit;
            if (existing_bit < new_bit) {
                node->children_.emplace_back(existing_leaf);
                node->children_.emplace_back(new_leaf);
            } else {
                node->children_.emplace_back(new_leaf);
                node->children_.emplace_back(existing_leaf);
            }
        }
        
        return node;
    }
    
    // Erase value from HAMT
    std::shared_ptr<Node> erase_impl(const std::shared_ptr<Node>& node, const Key& key, size_t hash, int level) const {
        if (!node) return nullptr;
        
        uint32_t bit = bitpos(hash, level);
        
        if (!(node->bitmap_ & bit)) {
            return node;  // Key doesn't exist
        }
        
        int idx = index(node->bitmap_, bit);
        auto& entry = node->children_[idx];
        
        if (entry.type_ == Entry::NODE) {
            auto new_child = erase_impl(entry.node_, key, hash, level + 1);
            if (new_child == entry.node_) {
                return node;  // No change
            }
            
            if (!new_child) {
                // Child was removed - need to remove from this node
                if (node->children_.size() == 1) {
                    return nullptr;  // This node becomes empty
                }
                auto new_node = std::make_shared<Node>(*node);
                new_node->bitmap_ &= ~bit;
                new_node->children_.erase(new_node->children_.begin() + idx);
                return new_node;
            }
            
            // Update child
            auto new_node = std::make_shared<Node>(*node);
            new_node->children_[idx] = Entry(new_child);
            return new_node;
        } else {
            // Check if leaf key matches
            if (key_equal_(entry.leaf_->value_.first, key)) {
                // Remove this leaf
                if (node->children_.size() == 1) {
                    return nullptr;  // Node becomes empty
                }
                auto new_node = std::make_shared<Node>(*node);
                new_node->bitmap_ &= ~bit;
                new_node->children_.erase(new_node->children_.begin() + idx);
                return new_node;
            }
            return node;  // Key not found
        }
    }
    
    // Collect all values for iteration (deterministic, no caching)
    void collect_values(const std::shared_ptr<Node>& node, std::vector<const std::pair<const Key, T>*>& result) const {
        if (!node) return;
        
        // Traverse in bitmap order (deterministic)
        for (uint32_t bit = 1; bit != 0; bit <<= 1) {
            if (node->bitmap_ & bit) {
                int idx = index(node->bitmap_, bit);
                auto& entry = node->children_[idx];
                
                if (entry.type_ == Entry::NODE) {
                    collect_values(entry.node_, result);
                } else {
                    result.push_back(&entry.leaf_->value_);
                }
            }
        }
    }

public:
    // Type definitions
    using key_type = Key;
    using mapped_type = T;
    using value_type = std::pair<const Key, T>;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using hasher = Hash;
    using key_equal = KeyEqual;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;

    // Forward declaration of iterators
    class iterator;
    class const_iterator;

    // Constructors
    ImmutableUnorderedMap() : root_(nullptr), size_(0) {}
    
    explicit ImmutableUnorderedMap(const Hash& hash, const KeyEqual& equal = KeyEqual())
        : root_(nullptr), size_(0), hasher_(hash), key_equal_(equal) {}

    // Copy constructor - O(1) shallow copy due to structural sharing
    ImmutableUnorderedMap(const ImmutableUnorderedMap& other)
        : root_(other.root_), size_(other.size_), hasher_(other.hasher_), key_equal_(other.key_equal_) {}

    // Move constructor
    ImmutableUnorderedMap(ImmutableUnorderedMap&& other) noexcept
        : root_(std::move(other.root_)), size_(other.size_),
          hasher_(std::move(other.hasher_)), key_equal_(std::move(other.key_equal_)) {
        other.size_ = 0;
    }

    // Copy assignment operator - O(1) shallow copy
    ImmutableUnorderedMap& operator=(const ImmutableUnorderedMap& other) {
        if (this != &other) {
            root_ = other.root_;
            size_ = other.size_;
            hasher_ = other.hasher_;
            key_equal_ = other.key_equal_;
        }
        return *this;
    }

    // Move assignment operator
    ImmutableUnorderedMap& operator=(ImmutableUnorderedMap&& other) noexcept {
        if (this != &other) {
            root_ = std::move(other.root_);
            size_ = other.size_;
            hasher_ = std::move(other.hasher_);
            key_equal_ = std::move(other.key_equal_);
            other.size_ = 0;
        }
        return *this;
    }

    // Lookup operations
    const T* find(const Key& key) const {
        if (!root_) return nullptr;
        size_type hash = hasher_(key);
        return find_impl(root_, key, hash, 0);
    }

    bool contains(const Key& key) const {
        return find(key) != nullptr;
    }

    std::optional<T> get(const Key& key) const {
        const T* value = find(key);
        if (value) {
            return *value;
        }
        return std::nullopt;
    }

    const T& at(const Key& key) const {
        const T* value = find(key);
        if (!value) {
            throw std::out_of_range("Key not found in ImmutableUnorderedMap");
        }
        return *value;
    }

    // Modifiers - return new maps with structural sharing
    ImmutableUnorderedMap insert(const Key& key, const T& value) const {
        bool existed = contains(key);
        ImmutableUnorderedMap result(*this);
        result.root_ = set_impl(root_, key, value, hasher_(key), 0);
        if (!existed) {
            result.size_ = size_ + 1;
        }
        return result;
    }
    
    ImmutableUnorderedMap insert(Key&& key, T&& value) const {
        Key key_copy = key;
        bool existed = contains(key_copy);
        ImmutableUnorderedMap result(*this);
        result.root_ = set_impl(root_, std::move(key), std::move(value), hasher_(key_copy), 0);
        if (!existed) {
            result.size_ = size_ + 1;
        }
        return result;
    }

    ImmutableUnorderedMap erase(const Key& key) const {
        if (!contains(key)) return *this;

        ImmutableUnorderedMap result(*this);
        result.root_ = erase_impl(root_, key, hasher_(key), 0);
        result.size_ = size_ - 1;
        return result;
    }

    // Capacity
    size_type size() const { return size_; }
    bool empty() const { return size_ == 0; }

    // Iterator implementation - deterministic traversal
    class const_iterator {
    private:
        std::vector<const value_type*> values_;
        size_t current_;
        
        void collect_all(const ImmutableUnorderedMap* map) {
            if (map && map->root_) {
                map->collect_values(map->root_, values_);
            }
        }

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::pair<const Key, T>;
        using difference_type = ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;

        const_iterator() : current_(0) {}
        
        const_iterator(const ImmutableUnorderedMap* map) : current_(0) {
            collect_all(map);
        }

        reference operator*() const {
            return *values_[current_];
        }

        pointer operator->() const {
            return values_[current_];
        }

        const_iterator& operator++() {
            ++current_;
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const const_iterator& other) const {
            // End iterators are equal if both are at end
            if (current_ >= values_.size() && other.current_ >= other.values_.size()) {
                return true;
            }
            // Same map and same position
            return current_ == other.current_ && values_ == other.values_;
        }

        bool operator!=(const const_iterator& other) const {
            return !(*this == other);
        }
    };

    // Iterator methods - only const iterators for immutable container
    const_iterator begin() const {
        return const_iterator(this);
    }

    const_iterator end() const {
        return const_iterator();
    }

    const_iterator cbegin() const {
        return const_iterator(this);
    }

    const_iterator cend() const {
        return const_iterator();
    }
};

} // namespace base
