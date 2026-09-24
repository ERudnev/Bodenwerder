#pragma once

#include <cstddef>
#include <iterator>
#include <optional>
#include <utility>

#include <fQSM/erased/delta.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/model/linear/changes.h>
#include <fQSM/model/linear/patch.h>
#include <fQSM/model/linear/state.h>

namespace fqsm::model::linear {

    template<category::Any Meta>
    struct Delta {
        using Key = Id<Meta>;
        using Value = Quantum<Meta>;
        using Layer = erased::DeltaLayer;
        enum class Mode {
            clean,
            dirty,
        };

        struct AsChange {
            static auto from(const erased::Change& change) -> Change<Key, Value> {
                return Change<Key, Value>{
                    Key{change.id},
                    change.tainted ? std::nullopt : std::optional<const Value*>{static_cast<const Value*>(change.before)},
                    static_cast<const Value*>(change.after),
                };
            }
        };
        struct AsAppeared {
            static auto from(const erased::Change& change) -> Appeared<Key, Value> {
                return {Key{change.id}, *static_cast<const Value*>(change.after)};
            }
        };
        struct AsUpdated {
            static auto from(const erased::Change& change) -> Updated<Key, Value> {
                return {Key{change.id}, *static_cast<const Value*>(change.before), *static_cast<const Value*>(change.after)};
            }
        };
        struct AsGone {
            static auto from(const erased::Change& change) -> Gone<Key, Value> {
                return {Key{change.id}, *static_cast<const Value*>(change.before)};
            }
        };
        struct AsUpserted {
            static auto from(const erased::Change& change) -> Upserted<Key, Value> {
                const auto* old = change.tainted ? nullptr : static_cast<const Value*>(change.before);
                return {Key{change.id}, old, *static_cast<const Value*>(change.after)};
            }
        };

        template<typename Projection>
        class Cursor {
        public:
            using iterator_category = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = decltype(Projection::from(std::declval<const erased::Change&>()));
            using pointer = void;
            using reference = value_type;

            Cursor() = default;
            explicit Cursor(erased::DeltaCursor inner) : inner(inner) {}

            value_type operator*() const { return Projection::from(*inner); }

            struct ArrowProxy {
                value_type view;
                const value_type* operator->() const { return &view; }
            };
            ArrowProxy operator->() const { return ArrowProxy{**this}; }
            Cursor& operator++() { ++inner; return *this; }
            Cursor operator++(int) { auto copy = *this; ++inner; return copy; }
            bool operator==(const Cursor& other) const { return inner == other.inner; }

        private:
            erased::DeltaCursor inner;
        };

        template<typename Projection>
        struct LayerView {
            using value_type = typename Cursor<Projection>::value_type;

            const Delta* owner;
            Layer layer;

            auto begin() const -> Cursor<Projection> { return Cursor<Projection>{owner->cursor_begin(layer)}; }
            auto end() const -> Cursor<Projection> { return Cursor<Projection>{owner->cursor_end(layer)}; }
            bool empty() const { return owner->layer_empty(layer); }
            std::size_t size() const { return static_cast<std::size_t>(std::distance(begin(), end())); }
        };

        using AllView = LayerView<AsChange>;
        using AppearedView = LayerView<AsAppeared>;
        using UpdatedView = LayerView<AsUpdated>;
        using GoneView = LayerView<AsGone>;
        using UpsertedView = LayerView<AsUpserted>;

        Delta(const State<Meta>& state, const Patch<Meta>& patch, Mode mode)
            : state(&state.line())
            , patch(&patch.view())
            , mode(mode == Mode::clean ? erased::DeltaMode::clean : erased::DeltaMode::dirty)
        {}

        auto begin() const -> Cursor<AsChange> { return all().begin(); }
        auto end() const -> Cursor<AsChange> { return all().end(); }
        auto all() const -> AllView { return AllView{this, Layer::all}; }
        auto added() const -> AppearedView { return AppearedView{this, Layer::added}; }
        auto addedOrUpdated() const -> UpsertedView { return UpsertedView{this, Layer::addedOrUpdated}; }
        auto removed() const -> GoneView { return GoneView{this, Layer::removed}; }
        auto updated() const -> UpdatedView { return UpdatedView{this, Layer::updated}; }
        bool empty() const { return layer_empty(Layer::all); }
        std::size_t size() const { return all().size(); }

    private:
        erased::DeltaCursor cursor_begin(Layer layer) const { return erased::DeltaCursor::begin(*state, *patch, mode, layer); }
        erased::DeltaCursor cursor_end(Layer layer) const { return erased::DeltaCursor::end(*state, *patch, mode, layer); }
        bool layer_empty(Layer layer) const { return erased::delta_empty(*state, *patch, mode, layer); }

        const erased::ReadLine* state;
        const erased::ReadPatch* patch;
        erased::DeltaMode mode;
    };
}
