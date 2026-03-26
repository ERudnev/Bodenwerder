#pragma once

#include <iQSM/delta.h>

namespace iqsm::internals {
    // Mutable accumulator for deltas (intended for Sequence/Accumulator/validation pipelines).
    struct FieldsMutable {
        using TypeId = ::iqsm::delta::FieldDiffAbstract::RuntimeTypeId;
        using Container = std::unordered_map<TypeId, ::iqsm::ref<::iqsm::delta::FieldDiffAbstract>>;

        Container fields;
        bool empty() const { return fields.empty(); }

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
                fields.erase(::iqsm::Facet<Meta>::typeId);
            }
        }

        template<meta::Aspect Meta>
        void set_global(typename ::iqsm::Facet<Meta>::Global before, typename ::iqsm::Facet<Meta>::Global after) {
            const auto fd = require_field<Meta>();

            if (fd->global_change.has_value()) {
                const auto& [lhs_before, lhs_after] = *fd->global_change;
                if (lhs_after != before) {
                    base::message("merge conflict resolved as last-wins");
                }
                fd->global_change = std::pair<typename ::iqsm::Facet<Meta>::Global, typename ::iqsm::Facet<Meta>::Global>{lhs_before, after};
            } else {
                fd->global_change = std::pair<typename ::iqsm::Facet<Meta>::Global, typename ::iqsm::Facet<Meta>::Global>{std::move(before), std::move(after)};
            }
        }

        void absorb(Delta rhs) {
            if (rhs->empty()) return;

            for (const auto& kv : rhs->fields) {
                const auto& type_id = kv.first;
                const auto& right = kv.second;

                auto it = fields.find(type_id);
                if (it == fields.end()) {
                    fields.emplace(type_id, right->clone());
                    continue;
                }

                it->second->merge(right);
                if (it->second->empty()) {
                    fields.erase(type_id);
                }
            }
        }

        // Returns an immutable snapshot without consuming state.
        // Costs: clones field diffs that are currently accumulated.
        Delta snapshot() const {
            if (fields.empty()) return ::iqsm::delta::empty();

            auto out = base::make_shared<::iqsm::delta::Fields>();
            out->fields.reserve(fields.size());

            for (const auto& kv : fields) {
                const auto& type_id = kv.first;
                const auto& field_diff = kv.second; // mutable
                out->fields.emplace(type_id, iqsm::freeze(field_diff->clone()));
            }

            return iqsm::freeze(out);
        }

        // Returns an immutable delta and resets the accumulator.
        // Moves owned mutable diffs into the immutable delta (no cloning).
        Delta push() {
            if (fields.empty()) return ::iqsm::delta::empty();

            auto out = base::make_shared<::iqsm::delta::Fields>();
            out->fields.reserve(fields.size());

            for (auto& kv : fields) {
                const auto& type_id = kv.first;
                auto& field_diff = kv.second; // mutable
                out->fields.emplace(type_id, iqsm::freeze(std::move(field_diff)));
            }

            fields.clear();
            return iqsm::freeze(out);
        }

    private:
        template<meta::Aspect Meta>
        ::iqsm::ref<::iqsm::delta::FieldDiff<Meta>> require_field() {
            const auto type_id = ::iqsm::Facet<Meta>::typeId;
            auto it = fields.find(type_id);
            if (it == fields.end()) {
                auto created = base::make_shared<::iqsm::delta::FieldDiff<Meta>>();
                fields.emplace(type_id, ::iqsm::ref<::iqsm::delta::FieldDiffAbstract>(created));
                return created;
            }
            return base::shared_ref_cast<::iqsm::delta::FieldDiff<Meta>>(it->second);
        }

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

