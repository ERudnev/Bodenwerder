#include <base/testing/runner.h>

#include <vector>

#define FQSM_FEATURES_TESTS(X) \
    X(structural_constraints) \
    X(direct_taint_feature_birth) \
    X(anchor_constraints) \
    X(entity_relations) \
    X(relations_index_build) \
    X(custom_reactions) \
    X(killing_feature) \
    X(cascade_closure) \
    X(inbound_index) \
    X(group_category) \
    X(group_performance) \
    X(cascade_performance) \
    X(relations_index_performance) \
    X(relations_watch_performance) \
    X(serialization) \
    X(destructor) \
    X(workers_say_no) \
    X(persistent_families) \
    X(temp_persistency) \
    X(remap_identities) \
    X(reactions_vocabulary) \
    // end

#define FQSM_LOW_LEVEL_TESTS(X) \
    X(containers_updated) \
    X(delta_iterators) \
    X(destructor_like_reactions) \
    X(globals) \
    X(global_assemble) \
    X(global_is_change_too) \
    X(immediate) \
    X(no_resurrection) \
    X(quantal) \
    X(transaction_hierarchy) \
    X(aspect_minimal_declaration) \
    X(id_set) \
    X(contexts_writing_copies_share_session) \
    X(contexts_nested_branch_refusal) \
    X(contexts_stewarding_direct_and_writing) \
    X(contexts_retrospecting_reads_origin) \
    X(nested_branch_meta_visibility) \
    X(schema_merge_nested_equals_flat) \
    X(schema_merge_order_independent) \
    X(schema_merge_single_fragment_identity) \
    X(schema_merge_duplicate_aspect_duplicates_custom_reactions) \
    X(schema_merge_realm_feature_removal_nested_vs_flat) \
    // end

#define FQSM_ERASED_TESTS(X) \
    X(erased_ops_trivial) \
    X(erased_ops_rich) \
    X(erased_ops_no_default) \
    X(erased_ops_aligned) \
    X(erased_describe) \
    X(erased_slots_basic) \
    X(erased_slots_aligned) \
    X(erased_slots_lifetime) \
    X(erased_line_basic) \
    X(erased_line_global_absent) \
    X(erased_line_lifetime) \
    X(erased_patch_line_operations) \
    X(erased_patch_line_touch) \
    X(erased_patch_line_absorb) \
    X(erased_patch_line_integrate_merge) \
    X(erased_patch_line_lifetime) \
    X(erased_overlay_nested) \
    X(erased_future_line_writes) \
    X(erased_delta_modes) \
    X(erased_items_view) \
    X(erased_items_future) \
    X(erased_lazy_patch_lines) \
    X(erased_pooled_lines) \
    X(structural_remove_with_parent) \
    X(structural_dead_parasitic_kills_parent) \
    X(structural_new_requires_existing_parent) \
    X(structural_new_requires_parent_appears) \
    X(structural_parent_appears_requires_component) \
    X(structural_group_removal_removes_elements) \
    X(structural_element_removal_unhooks) \
    X(structural_rule_with_absent_host_is_skipped) \
    // end

#define FQSM_MINIMODEL_TESTS(X) \
    // end

#define FQSM_Q1RUNTIME_TESTS(X) \
    X(schema_world_from_etalon) \
    // end

#define FQSM_WORKSHOP_TESTS(X) \
    X(polymorphic_behavior_exp) \
    // end

// synthetic workloads shaped like the game: benchmarks with population asserts
#define FQSM_WORKLOAD_TESTS(X) \
    X(workload_frame) \
    X(workload_load) \
    // end

BASETEST_FORWARD_DECLARE_TESTS(FQSM_FEATURES_TESTS)
BASETEST_FORWARD_DECLARE_TESTS(FQSM_LOW_LEVEL_TESTS)
BASETEST_FORWARD_DECLARE_TESTS(FQSM_ERASED_TESTS)
BASETEST_FORWARD_DECLARE_TESTS(FQSM_MINIMODEL_TESTS)
BASETEST_FORWARD_DECLARE_TESTS(FQSM_Q1RUNTIME_TESTS)
BASETEST_FORWARD_DECLARE_TESTS(FQSM_WORKSHOP_TESTS)
BASETEST_FORWARD_DECLARE_TESTS(FQSM_WORKLOAD_TESTS)

int call_all_tests() {
    struct group final {
        const char* name = "";
        std::vector<base::testing::test_case> tests{};
    };

    const std::vector<group> groups{
        group{ "features", BASETEST_MAKE_LIST_TESTS(FQSM_FEATURES_TESTS) },
        group{ "low_level", BASETEST_MAKE_LIST_TESTS(FQSM_LOW_LEVEL_TESTS) },
        group{ "erased", BASETEST_MAKE_LIST_TESTS(FQSM_ERASED_TESTS) },
        group{ "minimodel", BASETEST_MAKE_LIST_TESTS(FQSM_MINIMODEL_TESTS) },
        group{ "q1runtime", BASETEST_MAKE_LIST_TESTS(FQSM_Q1RUNTIME_TESTS) },
        group{ "workshop", BASETEST_MAKE_LIST_TESTS(FQSM_WORKSHOP_TESTS) },
        group{ "workload", BASETEST_MAKE_LIST_TESTS(FQSM_WORKLOAD_TESTS) },
    };

    base::testing::run_summary total{};

    for (std::size_t i = 0; i < groups.size(); ++i) {
        if (i != 0) base::message("");
        base::message(std::format("{}:", groups[i].name));

        const auto s = base::testing::run_tests(groups[i].tests);
        total += s;
    }

    base::message("");
    base::message(std::format("TOTAL SUMMARY: {} passed={}, failed={}, total={}",
        total.ok() ? "OK" : "FAIL",
        total.passed,
        total.failed,
        total.total()
    ));
    base::message(std::format("TOTAL TIME: {:.3f} ms", total.elapsed_ms()));

    return total.ok() ? 0 : 1;
}

int call_specific_test() {
    const auto s = base::testing::run_tests(
        BASETEST_LIST(BASETEST_NAMED("selected", &tests::temp_persistency)));
    return s.ok() ? 0 : 1;
}

int main() {
    //return call_specific_test();
    return call_all_tests();
};
