#pragma once

#include <concepts>
#include <iterator>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <fQSM/identifier.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/processing/contexts/session.h>

namespace fqsm::manipulation {

    namespace detail {

        // Target::Id → Watcher ids (scoped to a delta layer of Target).
        //   .ids(target)   → cheap id range (empty if none)
        //   .items(target) → {id, const Quantum&} via get
        template<category::Any Target, category::Any Watchers>
        struct InboundIndex {
            using TargetId = typename Target::Id;
            using WatcherId = typename Watchers::Id;
            using Quantum = typename Watchers::Quantum;
            using Map = std::unordered_map<TargetId, std::vector<WatcherId>>;

            Reading context;
            Map by_target;

            struct Related {
                WatcherId id;
                const Quantum& item;
            };

            struct RelatedRange {
                Reading context;
                const std::vector<WatcherId>* ids = nullptr;

                struct iterator {
                    using iterator_category = std::forward_iterator_tag;
                    using value_type = Related;
                    using difference_type = std::ptrdiff_t;
                    using pointer = void;
                    using reference = Related;

                    Reading context;
                    const WatcherId* current = nullptr;

                    auto operator*() const -> Related {
                        return Related{
                            *current,
                            meta::facade_t<Watchers>::get(context, *current),
                        };
                    }

                    auto operator++() -> iterator& {
                        ++current;
                        return *this;
                    }

                    friend auto operator==(const iterator& a, const iterator& b) -> bool {
                        return a.current == b.current;
                    }
                };

                auto begin() const -> iterator {
                    if (ids == nullptr or ids->empty())
                        return iterator{context, nullptr};
                    return iterator{context, ids->data()};
                }

                auto end() const -> iterator {
                    if (ids == nullptr or ids->empty())
                        return iterator{context, nullptr};
                    return iterator{context, ids->data() + ids->size()};
                }
            };

            auto ids(TargetId id) const -> const std::vector<WatcherId>& {
                const auto* bucket = bucket_for(id);
                if (bucket == nullptr) {
                    static const std::vector<WatcherId> empty{};
                    return empty;
                }
                return *bucket;
            }

            auto items(TargetId id) const -> RelatedRange {
                return RelatedRange{context, bucket_for(id)};
            }

        private:
            auto bucket_for(TargetId id) const -> const std::vector<WatcherId>* {
                const auto found = by_target.find(id);
                if (found == by_target.end())
                    return nullptr;
                return &found->second;
            }
        };

        template<typename Meta>
        auto link_key(const ::fqsm::Identifier<Meta>& id) -> std::optional<::fqsm::Identifier<Meta>> {
            return id;
        }

        template<typename Meta>
        auto link_key(const ::fqsm::Affected<Meta>& id) -> std::optional<::fqsm::Identifier<Meta>> {
            return static_cast<const ::fqsm::Identifier<Meta>&>(id);
        }

        template<typename Meta>
        auto link_key(const std::optional<::fqsm::Identifier<Meta>>& id)
            -> std::optional<::fqsm::Identifier<Meta>>
        {
            return id;
        }

        // The reader of one link field, erased. Its address identifies the link in the schema (one instantiation per
        // Watchers and Link in the program), so the reaction that declares the link and the query that uses it agree.
        template<category::Any Watchers, auto Link>
        RawId read_link(const void* quantum) {
            const auto key = link_key(static_cast<const typename Watchers::Quantum*>(quantum)->*Link);
            return key.has_value() ? key->raw() : RawId{0};
        }

        template<typename Target, typename LinkT>
        concept InboundLinkValue =
            std::same_as<LinkT, typename Target::Id>
            or std::same_as<LinkT, ::fqsm::Affected<Target>>
            or std::same_as<LinkT, std::optional<typename Target::Id>>;

        // ask::relations<Target>(reacting).removed|updated|added|addedOrUpdated<Watchers, link>()
        // Link: Affected / Id / optional<Id> (Anchor/Custody are Id aliases).
        // Index covers only watchers linked to Target ids present in that delta layer;
        // empty layer → empty index (no Watchers scan). nullopt links are skipped.
        template<category::Any Target>
        struct RelationsOf {
            Reacting context;

            explicit RelationsOf(Reacting context) : context(std::move(context)) {}

            template<category::Any Watchers, auto Link>
                requires InboundLinkValue<Target, std::remove_cvref_t<decltype(std::declval<typename Watchers::Quantum>().*Link)>>
            auto removed() const -> InboundIndex<Target, Watchers> {
                return index_for_layer<Watchers, Link>(context.changes<Target>().removed());
            }

            template<category::Any Watchers, auto Link>
                requires InboundLinkValue<Target, std::remove_cvref_t<decltype(std::declval<typename Watchers::Quantum>().*Link)>>
            auto updated() const -> InboundIndex<Target, Watchers> {
                return index_for_layer<Watchers, Link>(context.changes<Target>().updated());
            }

            template<category::Any Watchers, auto Link>
                requires InboundLinkValue<Target, std::remove_cvref_t<decltype(std::declval<typename Watchers::Quantum>().*Link)>>
            auto added() const -> InboundIndex<Target, Watchers> {
                return index_for_layer<Watchers, Link>(context.changes<Target>().added());
            }

            template<category::Any Watchers, auto Link>
                requires InboundLinkValue<Target, std::remove_cvref_t<decltype(std::declval<typename Watchers::Quantum>().*Link)>>
            auto addedOrUpdated() const -> InboundIndex<Target, Watchers> {
                return index_for_layer<Watchers, Link>(context.changes<Target>().addedOrUpdated());
            }

        private:
            template<category::Any Watchers, auto Link, typename LayerView>
            auto index_for_layer(LayerView layer) const -> InboundIndex<Target, Watchers> {
                InboundIndex<Target, Watchers> index{.context = context};
                if (layer.empty())
                    return index;

                using TargetId = typename Target::Id;
                using WatcherId = typename Watchers::Id;
                std::unordered_set<TargetId> interesting;
                for (const auto& change : layer)
                    interesting.insert(change.id);

                // Indexed: the Reality's inbound index for this link, corrected by the transaction's own layers.
                // A holder mentioned in a pending layer is judged by its pending value; the index answers for the rest.
                // A direct pass over the watchers may have changed links in place: then the index is stale until the
                // transaction ends, and the scan below answers.
                const auto& proposal = context.proposal;
                const auto& schema = *proposal.schema;
                const auto watchers = schema.slotOf(TypeId<Watchers>);
                const auto* indexes = proposal.inbound();
                const auto link = indexes ? schema.linkOf(watchers, schema.slotOf(TypeId<Target>), &read_link<Watchers, Link>) : schema.npos;
                if (link != schema.npos and not proposal.tainted(watchers)) {
                    std::vector<const erased::PatchLine*> layers;
                    proposal.pending_layers(watchers, layers);
                    std::unordered_map<RawId, const void*> pending;   // topmost layer wins; nullptr: deleted
                    for (const auto* line : layers)
                        for (std::size_t i = 0; i < line->count(); ++i) {
                            const auto patchlet = line->at(i);
                            pending.try_emplace(line->id_at(i), patchlet.tombstone ? nullptr : patchlet.value);
                        }
                    const auto& inbound = (*indexes)[link];
                    for (const auto target : interesting)
                        if (const auto* holders = inbound.holders(target.raw()))
                            for (const RawId holder : *holders)
                                if (not pending.contains(holder))
                                    index.by_target[target].push_back(WatcherId{holder});
                    for (const auto& [holder, value] : pending) {
                        if (not value) continue;
                        const RawId target = read_link<Watchers, Link>(value);
                        if (target and interesting.contains(TargetId{target}))
                            index.by_target[TargetId{target}].push_back(WatcherId{holder});
                    }
                    return index;
                }

                const Reading reading = context;
                for (const auto entry : reading->template aspect<Watchers>().items()) {
                    const auto key = link_key(entry.value.*Link);
                    if (not key.has_value())
                        continue;
                    if (interesting.contains(*key))
                        index.by_target[*key].push_back(entry.id);
                }
                return index;
            }
        };

    } // namespace detail

    template<category::Any Target>
    auto relations(Reacting context) -> detail::RelationsOf<Target> {
        return detail::RelationsOf<Target>{std::move(context)};
    }

}
