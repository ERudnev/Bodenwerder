#pragma once

#include <iQSM/delta.h>
#include <iQSM/meta/aspect_id.h>
#include <iQSM/schema.h>

namespace iqsm::internals {
    // Thin mutable facade over Delta for typed field/global operations.
    struct FieldsMutable {
        FieldsMutable() = default;
        explicit FieldsMutable(Delta delta_) : delta_storage(std::move(delta_)) {}

        bool empty() const { return fields().empty(); }
        Delta delta() const { return delta_storage; }

        template<meta::Aspect Meta>
        void add_op(::iqsm::Id<Meta> id, typename ::iqsm::delta::FieldDiff<Meta>::Operation op) {
            using FieldDiff = ::iqsm::delta::FieldDiff<Meta>;
            using Operation = typename FieldDiff::Operation;

            if (op.is_noop()) return;

            const auto fd = require_field<Meta>();

            auto it = fd->ops.find(id);
            if (it == fd->ops.end()) {
                fd->ops.emplace(std::move(id), std::move(op));
            } else {
                Operation merged = merge_ops<Meta>(it->second, op);
                if (merged.is_noop()) {
                    fd->ops.erase(it);
                } else {
                    it->second = std::move(merged);
                }
            }

            if (fd->empty()) {
                fields().erase(::iqsm::types::aspectId<Meta>());
            }
        }

        template<meta::Aspect Meta>
        void set_global(typename ::iqsm::delta::FieldDiff<Meta>::Global before, typename ::iqsm::delta::FieldDiff<Meta>::Global after) {
            const auto fd = require_field<Meta>();

            if (fd->global_change.has_value()) {
                const auto& [lhs_before, lhs_after] = *fd->global_change;
                if (lhs_after != before) {
                    base::message("merge conflict resolved as last-wins");
                }
                fd->global_change = std::pair<typename ::iqsm::delta::FieldDiff<Meta>::Global, typename ::iqsm::delta::FieldDiff<Meta>::Global>{lhs_before, after};
            } else {
                fd->global_change = std::pair<typename ::iqsm::delta::FieldDiff<Meta>::Global, typename ::iqsm::delta::FieldDiff<Meta>::Global>{std::move(before), std::move(after)};
            }
        }

        void absorb(Schema schema, Delta rhs) {
            if (rhs->empty()) return;

            for (auto& kv : rhs->fields) {
                const auto& type_id = kv.first;
                auto& right = kv.second;

                if (not schema->aspects.contains(type_id)) {
                    throw std::runtime_error("FieldsMutable::absorb(): delta contains type not in schema");
                }
                const auto& ops = schema->aspects.at(type_id).delta;
                if (not ops.absorb || not ops.empty) {
                    throw std::runtime_error("FieldsMutable::absorb(): schema entry has no delta thunks");
                }

                auto it = fields().find(type_id);
                if (it == fields().end()) {
                    fields().emplace(type_id, std::move(right));
                    continue;
                }

                ops.absorb(it->second, right);
                if (ops.empty(it->second)) {
                    fields().erase(type_id);
                }
            }
        }

        // Returns a mutable delta and resets the accumulator.
        // Moves owned mutable diffs into the outgoing delta (no cloning).
        Delta push() {
            auto out = std::move(delta_storage);
            delta_storage = ::iqsm::delta::empty();
            return out;
        }

    private:
        auto fields() -> ::iqsm::delta::Fields::Container& { return delta_storage->fields; }
        auto fields() const -> const ::iqsm::delta::Fields::Container& { return delta_storage->fields; }

        template<meta::Aspect Meta>
        ::iqsm::ref<::iqsm::delta::FieldDiff<Meta>> require_field() {
            const auto type_id = ::iqsm::types::aspectId<Meta>();
            auto it = fields().find(type_id);
            if (it == fields().end()) {
                auto created = base::make_shared<::iqsm::delta::FieldDiff<Meta>>();
                fields().emplace(type_id, ::iqsm::ref<::iqsm::delta::FieldDiffAbstract>(created));
                return created;
            }
            return base::shared_ref_cast<::iqsm::delta::FieldDiff<Meta>>(it->second);
        }

        Delta delta_storage = ::iqsm::delta::empty();

        template<meta::Aspect Meta>
        static typename ::iqsm::delta::FieldDiff<Meta>::Operation merge_ops(
            const typename ::iqsm::delta::FieldDiff<Meta>::Operation& lhs,
            const typename ::iqsm::delta::FieldDiff<Meta>::Operation& rhs)
        {
            using Operation = typename ::iqsm::delta::FieldDiff<Meta>::Operation;

            if (rhs.is_noop()) return lhs;
            if (lhs.is_noop()) return rhs;

            const auto report = []() {
                base::message("merge conflict resolved by unique-id policy");
            };

            if (lhs.is_chg() && rhs.is_chg()) {
                if (lhs.after && rhs.before && *lhs.after != *rhs.before) {
                    report();
                }
                return Operation{lhs.before, rhs.after};
            }

            if (lhs.is_del() || rhs.is_del()) {
                const auto& del   = lhs.is_del() ? lhs : rhs;
                const auto& other = lhs.is_del() ? rhs : lhs;

                auto before = del.before;

                if (!before) {
                    if (other.is_chg()) before = other.after;
                    else if (other.is_add()) before = other.after;
                    else if (other.is_del()) before = other.before;
                } else {
                    if (other.is_chg() && other.after && *before != *other.after) report();
                    if (other.is_add() && other.after && *before != *other.after) report();
                    if (other.is_del() && other.before && *before != *other.before) report();
                }

                return Operation{before, std::nullopt};
            }

            if (lhs.is_add() || rhs.is_add()) {
                const auto& add   = lhs.is_add() ? lhs : rhs;
                const auto& other = lhs.is_add() ? rhs : lhs;

                if (other.is_add()) {
                    if (add.after && other.after && *add.after != *other.after) {
                        report();
                    }
                    return Operation{std::nullopt, rhs.after};
                }

                if (other.is_chg()) {
                    if (add.after && other.before && *add.after != *other.before) {
                        report();
                    }
                    return Operation{std::nullopt, other.after};
                }

                report();
                return Operation{std::nullopt, rhs.after};
            }

            report();
            return rhs;
        }
    };
}

