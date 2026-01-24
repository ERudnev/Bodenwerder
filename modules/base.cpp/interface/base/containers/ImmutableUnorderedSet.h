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

template<typename T, typename Hash = std::hash<T>, typename KeyEqual = std::equal_to<T>>
class ImmutableUnorderedSet {
private:
    // Forward declaration
    struct Node;
    
    // Leaf node for storing actual values
    struct Leaf {
        T value_;
        
        Leaf(const T& value) : value_(value) {}
        Leaf(T&& value) : value_(std::move(value)) {}
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
    const T* find_impl(const std::shared_ptr<Node>& node, const T& value, size_t hash, int level) const {
        if (!node) return nullptr;
        
        uint32_t bit = bitpos(hash, level);
        
        if (!(node->bitmap_ & bit)) {
            return nullptr;  // Bit not set, value doesn't exist
        }
        
        int idx = index(node->bitmap_, bit);
        auto& entry = node->children_[idx];
        
        if (entry.type_ == Entry::NODE) {
            // Recursive search in internal node
            return find_impl(entry.node_, value, hash, level + 1);
        } else {
            // Check leaf
            if (key_equal_(entry.leaf_->value_, value)) {
                return &entry.leaf_->value_;
            }
            // Hash collision - value not found
            return nullptr;
        }
    }
    
    // Insert value into HAMT (returns new node, or same node if no change)
    std::shared_ptr<Node> insert_impl(const std::shared_ptr<Node>& node, const T& value, size_t hash, int level) const {
        if (!node) {
            // Create new leaf node
            auto leaf = std::make_shared<Leaf>(value);
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
                // Recursive insert
                auto new_child = insert_impl(entry.node_, value, hash, level + 1);
                if (new_child == entry.node_) {
                    return node;  // No change
                }
                // Create new node with updated child
                auto new_node = std::make_shared<Node>(*node);
                new_node->children_[idx] = Entry(new_child);
                return new_node;
            } else {
                // Collision with existing leaf
                if (key_equal_(entry.leaf_->value_, value)) {
                    return node;  // Value already exists
                }
                
                // Hash collision - need to create internal node
                // Move existing leaf deeper and insert both
                size_t existing_hash = hasher_(entry.leaf_->value_);
                auto new_node = std::make_shared<Node>(*node);
                new_node->children_[idx] = Entry(create_collision_node(
                    entry.leaf_, std::make_shared<Leaf>(value), 
                    existing_hash, hash, level + 1));
                return new_node;
            }
        } else {
            // Position free - insert leaf directly
            auto new_node = std::make_shared<Node>(*node);
            new_node->bitmap_ |= bit;
            int idx = index(node->bitmap_, bit);
            auto leaf = std::make_shared<Leaf>(value);
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
    std::shared_ptr<Node> erase_impl(const std::shared_ptr<Node>& node, const T& value, size_t hash, int level) const {
        if (!node) return nullptr;
        
        uint32_t bit = bitpos(hash, level);
        
        if (!(node->bitmap_ & bit)) {
            return node;  // Value doesn't exist
        }
        
        int idx = index(node->bitmap_, bit);
        auto& entry = node->children_[idx];
        
        if (entry.type_ == Entry::NODE) {
            auto new_child = erase_impl(entry.node_, value, hash, level + 1);
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
            // Check if leaf matches
            if (key_equal_(entry.leaf_->value_, value)) {
                // Remove this leaf
                if (node->children_.size() == 1) {
                    return nullptr;  // Node becomes empty
                }
                auto new_node = std::make_shared<Node>(*node);
                new_node->bitmap_ &= ~bit;
                new_node->children_.erase(new_node->children_.begin() + idx);
                return new_node;
            }
            return node;  // Value not found
        }
    }
    
    // Collect all values for iteration (deterministic, no caching)
    void collect_values(const std::shared_ptr<Node>& node, std::vector<const T*>& result) const {
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
    using value_type = T;
    using key_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using hasher = Hash;
    using key_equal = KeyEqual;
    using reference = const T&;
    using const_reference = const T&;
    using pointer = const T*;
    using const_pointer = const T*;

    // Forward declaration of iterators
    class iterator;
    class const_iterator;

    // Constructors
    ImmutableUnorderedSet() : root_(nullptr), size_(0) {}
    
    explicit ImmutableUnorderedSet(const Hash& hash, const KeyEqual& equal = KeyEqual())
        : root_(nullptr), size_(0), hasher_(hash), key_equal_(equal) {}

    // Copy constructor - O(1) shallow copy due to structural sharing
    ImmutableUnorderedSet(const ImmutableUnorderedSet& other)
        : root_(other.root_), size_(other.size_), hasher_(other.hasher_), key_equal_(other.key_equal_) {}

    // Move constructor
    ImmutableUnorderedSet(ImmutableUnorderedSet&& other) noexcept
        : root_(std::move(other.root_)), size_(other.size_),
          hasher_(std::move(other.hasher_)), key_equal_(std::move(other.key_equal_)) {
        other.size_ = 0;
    }

    // Copy assignment operator - O(1) shallow copy
    ImmutableUnorderedSet& operator=(const ImmutableUnorderedSet& other) {
        if (this != &other) {
            root_ = other.root_;
            size_ = other.size_;
            hasher_ = other.hasher_;
            key_equal_ = other.key_equal_;
        }
        return *this;
    }

    // Move assignment operator
    ImmutableUnorderedSet& operator=(ImmutableUnorderedSet&& other) noexcept {
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
    bool contains(const T& value) const {
        if (!root_) return false;
        size_type hash = hasher_(value);
        return find_impl(root_, value, hash, 0) != nullptr;
    }

    const T* find(const T& value) const {
        if (!root_) return nullptr;
        size_type hash = hasher_(value);
        return find_impl(root_, value, hash, 0);
    }

    // Modifiers - return new sets with structural sharing
    ImmutableUnorderedSet insert(const T& value) const {
        if (contains(value)) return *this;
        
        ImmutableUnorderedSet result(*this);
        result.root_ = insert_impl(root_, value, hasher_(value), 0);
        result.size_ = size_ + 1;
        return result;
    }
    
    ImmutableUnorderedSet insert(T&& value) const {
        // Check first to avoid moving if already exists
        T temp = value;
        if (contains(temp)) return *this;
        
        ImmutableUnorderedSet result(*this);
        result.root_ = insert_impl(root_, std::move(value), hasher_(value), 0);
        result.size_ = size_ + 1;
        return result;
    }

    ImmutableUnorderedSet erase(const T& value) const {
        if (!contains(value)) return *this;

        ImmutableUnorderedSet result(*this);
        result.root_ = erase_impl(root_, value, hasher_(value), 0);
        result.size_ = size_ - 1;
        return result;
    }

    // Capacity
    size_type size() const { return size_; }
    bool empty() const { return size_ == 0; }

    // Iterator implementation - deterministic traversal
    class const_iterator {
    private:
        std::vector<const T*> values_;
        size_t current_;
        
        void collect_all(const ImmutableUnorderedSet* set) {
            if (set && set->root_) {
                set->collect_values(set->root_, values_);
            }
        }

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

        const_iterator() : current_(0) {}
        
        const_iterator(const ImmutableUnorderedSet* set) : current_(0) {
            collect_all(set);
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
            // Same set and same position
            return current_ == other.current_ && values_ == other.values_;
        }

        bool operator!=(const const_iterator& other) const {
            return !(*this == other);
        }
    };

    // Iterator methods
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
