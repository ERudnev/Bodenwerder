#include <fQSM/erased/algorithms.h>

#include <format>

#include <fQSM/erased/delta.h>

namespace fqsm::erased {

    void integrate(Line& target, const PatchLine& patch) {
        if (const void* global = patch.global())
            target.set_global(global);
        for (std::size_t i = 0; i < patch.count(); ++i) {
            const auto patchlet = patch.at(i);
            if (patchlet.tombstone)
                target.erase(patch.id_at(i));
            else
                target.insert(patch.id_at(i), patchlet.value);
        }
    }

    void integrate_move(Line& target, PatchLine& patch) {
        if (const void* global = patch.global())
            target.set_global(global);
        for (std::size_t i = 0; i < patch.count(); ++i) {
            const auto patchlet = patch.at(i);
            if (patchlet.tombstone)
                target.erase(patch.id_at(i));
            else
                target.emplace_move(patch.id_at(i), patch.mutable_at(i));
        }
    }

    void merge_into(const ReadLine& base, PatchLine& target, const PatchLine& source) {
        if (const void* global = source.global())
            target.set_global(global);
        const auto end = DeltaCursor::end(base, source, DeltaMode::clean, DeltaLayer::all);
        for (auto it = DeltaCursor::begin(base, source, DeltaMode::clean, DeltaLayer::all); not (it == end); ++it) {
            const auto change = *it;
            if (change.add() or change.update())
                target.modify(change.id, change.after);
            if (change.remove())
                target.del(change.id, change.before);
        }
    }

    std::string format_patch_line(const PatchLine& patch, std::string_view name) {
        std::string chain;
        if (patch.global())
            chain += "[global, ??]";
        for (std::size_t i = 0; i < patch.count(); ++i) {
            if (not chain.empty()) chain += ' ';
            chain += std::format("[#{}, {}]", internal::id::info_hash(patch.id_at(i)), patch.at(i).tombstone ? "del" : "??");
        }
        if (chain.empty()) return {};
        return std::format("{} {}", name, chain);
    }
}
