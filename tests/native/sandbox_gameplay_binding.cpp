#include <tgd/contracts/sandbox_gameplay_binding.hpp>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <span>
#include <string_view>
#include <type_traits>

namespace {

using namespace tgd::contracts;

template <typename Type>
concept HasGenericTarget = requires(Type value) { value.target; };

template <typename Type>
concept HasSafePointTarget = requires(Type value) { value.target_safe_point_id; };

template <typename Type>
concept HasToggle = requires(Type value) { value.toggle; };

template <typename Type>
concept HasMultipleTargets = requires(Type value) { value.target_ids; };

template <typename Type>
concept HasPose = requires(Type value) { value.pose; };

template <typename Type>
concept HasComponentResult = requires(Type value) {
    value.interaction_result;
    value.mechanism_result;
    value.blocker_result;
};

template <typename Type>
concept HasStoredEvents = requires(Type value) {
    value.events;
    value.event_count;
};

template <typename Type>
concept HasMutableSideEffects = requires(Type value) {
    value.state;
    value.checksum;
    value.progress;
    value.reward;
};

static_assert(sandbox_operate_range_min_mm == 500);
static_assert(sandbox_operate_range_max_mm == 3'000);
static_assert(static_cast<std::uint8_t>(SandboxInteractionOperation::operate) == 1);
static_assert(static_cast<std::uint8_t>(SandboxInteractionOperation::invalid) == 255);
static_assert(static_cast<std::uint8_t>(SandboxMechanismActivation::one_shot_activate) == 1);
static_assert(static_cast<std::uint8_t>(SandboxMechanismActivation::invalid) == 255);
static_assert(static_cast<std::uint8_t>(SandboxInteractionState::uncompleted) == 1);
static_assert(static_cast<std::uint8_t>(SandboxInteractionState::completed) == 2);
static_assert(static_cast<std::uint8_t>(SandboxInteractionState::invalid) == 255);
static_assert(static_cast<std::uint8_t>(SandboxMechanismState::inactive) == 1);
static_assert(static_cast<std::uint8_t>(SandboxMechanismState::activated) == 2);
static_assert(static_cast<std::uint8_t>(SandboxMechanismState::invalid) == 255);
static_assert(static_cast<std::uint8_t>(SandboxGroundBlockerState::enabled_solid) == 1);
static_assert(static_cast<std::uint8_t>(SandboxGroundBlockerState::disabled_non_solid) == 2);
static_assert(static_cast<std::uint8_t>(SandboxGroundBlockerState::invalid) == 255);
static_assert(static_cast<std::uint8_t>(SandboxGameplayEventKind::interaction_completed) == 1);
static_assert(static_cast<std::uint8_t>(SandboxGameplayEventKind::mechanism_activated) == 2);
static_assert(static_cast<std::uint8_t>(SandboxGameplayEventKind::invalid) == 255);
static_assert(static_cast<std::uint8_t>(SandboxOperateDisposition::completed_chain) == 1);
static_assert(static_cast<std::uint8_t>(SandboxOperateDisposition::repeated_chain) == 2);
static_assert(static_cast<std::uint8_t>(SandboxOperateDisposition::stale_generation) == 6);
static_assert(static_cast<std::uint8_t>(SandboxOperateDisposition::stale_sequence) == 7);
static_assert(static_cast<std::uint8_t>(SandboxOperateDisposition::invalid_binding) == 8);
static_assert(static_cast<std::uint8_t>(SandboxOperateDisposition::invalid) == 255);
static_assert(static_cast<std::uint8_t>(SandboxOperateRangeCheck::eligible) == 1);
static_assert(static_cast<std::uint8_t>(SandboxOperateRangeCheck::out_of_range) == 4);
static_assert(static_cast<std::uint8_t>(SandboxOperateRangeCheck::invalid) == 255);
static_assert(static_cast<std::uint8_t>(SandboxGameplayBindingValidationCode::valid) == 1);
static_assert(
    static_cast<std::uint8_t>(SandboxGameplayBindingValidationCode::unreferenced_mechanism) == 21
);
static_assert(static_cast<std::uint8_t>(SandboxGameplayBindingValidationCode::invalid) == 255);
static_assert(
    static_cast<std::uint8_t>(
        SandboxGameplayBindingValidationDomain::interaction_binding
    ) == 1
);
static_assert(
    static_cast<std::uint8_t>(SandboxGameplayBindingValidationDomain::core_mechanism) == 4
);
static_assert(static_cast<std::uint8_t>(SandboxGameplayBindingValidationDomain::invalid) == 255);
static_assert(
    static_cast<std::uint16_t>(
        SandboxGameplayBindingValidationField::target_ground_blocker_id
    ) == 7
);
static_assert(
    static_cast<std::uint16_t>(SandboxGameplayBindingValidationField::invalid) == 65'535
);

static_assert(sandbox_interaction_initial_state == SandboxInteractionState::uncompleted);
static_assert(sandbox_mechanism_initial_state == SandboxMechanismState::inactive);
static_assert(
    sandbox_ground_blocker_initial_state == SandboxGroundBlockerState::enabled_solid
);

constexpr SandboxOperateResult completed{SandboxOperateDisposition::completed_chain};
constexpr SandboxOperateResult repeated{SandboxOperateDisposition::repeated_chain};
constexpr SandboxOperateResult rejected{SandboxOperateDisposition::stale_generation};
constexpr SandboxOperateResult rejected_sequence{SandboxOperateDisposition::stale_sequence};
constexpr SandboxOperateResult raw_zero{static_cast<SandboxOperateDisposition>(0)};
constexpr SandboxOperateResult raw_255{static_cast<SandboxOperateDisposition>(255)};

static_assert(
    sandbox_interaction_operate_result(completed) ==
    SandboxInteractionOperateResult::completed
);
static_assert(
    sandbox_mechanism_activate_result(completed) == SandboxMechanismActivateResult::activated
);
static_assert(
    sandbox_ground_blocker_write_result(completed) ==
    SandboxGroundBlockerWriteResult::disabled
);
static_assert(sandbox_operate_event_count(completed) == 2);
static_assert(
    sandbox_operate_event_at(completed, 0) == SandboxGameplayEventKind::interaction_completed
);
static_assert(
    sandbox_operate_event_at(completed, 1) == SandboxGameplayEventKind::mechanism_activated
);
static_assert(sandbox_operate_event_at(completed, 2) == SandboxGameplayEventKind::invalid);

static_assert(
    sandbox_interaction_operate_result(repeated) ==
    SandboxInteractionOperateResult::already_completed
);
static_assert(
    sandbox_mechanism_activate_result(repeated) ==
    SandboxMechanismActivateResult::already_activated
);
static_assert(
    sandbox_ground_blocker_write_result(repeated) ==
    SandboxGroundBlockerWriteResult::ignored_repeat
);
static_assert(sandbox_operate_event_count(repeated) == 0);
static_assert(sandbox_operate_event_at(repeated, 0) == SandboxGameplayEventKind::invalid);
static_assert(
    sandbox_interaction_operate_result(rejected) ==
    SandboxInteractionOperateResult::not_applied
);
static_assert(sandbox_operate_event_count(rejected) == 0);
static_assert(
    sandbox_interaction_operate_result(rejected_sequence) ==
    SandboxInteractionOperateResult::not_applied
);
static_assert(
    sandbox_mechanism_activate_result(rejected_sequence) ==
    SandboxMechanismActivateResult::not_applied
);
static_assert(
    sandbox_ground_blocker_write_result(rejected_sequence) ==
    SandboxGroundBlockerWriteResult::not_applied
);
static_assert(sandbox_operate_event_count(rejected_sequence) == 0);
static_assert(sandbox_interaction_operate_result(raw_zero) == SandboxInteractionOperateResult::invalid);
static_assert(sandbox_interaction_operate_result(raw_255) == SandboxInteractionOperateResult::invalid);
static_assert(sandbox_operate_event_count(raw_zero) == 0);
static_assert(sandbox_operate_event_count(raw_255) == 0);

static_assert(!HasGenericTarget<SandboxInteractionGameplayBinding>);
static_assert(!HasGenericTarget<SandboxMechanismGameplayBinding>);
static_assert(!HasSafePointTarget<SandboxInteractionGameplayBinding>);
static_assert(!HasSafePointTarget<SandboxMechanismGameplayBinding>);
static_assert(!HasToggle<SandboxMechanismGameplayBinding>);
static_assert(!HasMultipleTargets<SandboxInteractionGameplayBinding>);
static_assert(!HasMultipleTargets<SandboxMechanismGameplayBinding>);
static_assert(!HasPose<SandboxOperateCommand>);
static_assert(!HasComponentResult<SandboxOperateResult>);
static_assert(!HasStoredEvents<SandboxOperateResult>);
static_assert(!HasMutableSideEffects<SandboxOperateResult>);
static_assert(std::is_aggregate_v<SandboxOperateCommand>);
static_assert(std::is_aggregate_v<SandboxOperateResult>);

static_assert(
    !sandbox_gameplay_binding_is_valid({
        static_cast<SandboxGameplayBindingValidationCode>(0),
        0,
        0,
        0,
        SandboxGameplayBindingValidationDomain::invalid,
        SandboxGameplayBindingValidationField::invalid,
    })
);
static_assert(
    !sandbox_gameplay_binding_is_valid({
        static_cast<SandboxGameplayBindingValidationCode>(255),
        0,
        0,
        0,
        SandboxGameplayBindingValidationDomain::invalid,
        SandboxGameplayBindingValidationField::invalid,
    })
);

constexpr ContentId interaction_one = content_id("sandbox.interaction.one");
constexpr ContentId interaction_two = content_id("sandbox.interaction.two");
constexpr ContentId mechanism_one = content_id("sandbox.mechanism.one");
constexpr ContentId mechanism_two = content_id("sandbox.mechanism.two");
constexpr ContentId blocker_one = content_id("sandbox.blocker.one");
constexpr ContentId blocker_two = content_id("sandbox.blocker.two");
constexpr ContentId safe_point_one = content_id("sandbox.safe-point.one");
constexpr ContentId extra_id = content_id("sandbox.extra");

SandboxInteractionDefinition interaction_definition(ContentId id) {
    SandboxInteractionDefinition result{};
    result.id = id;
    return result;
}

SandboxMechanismDefinition mechanism_definition(ContentId id) {
    SandboxMechanismDefinition result{};
    result.id = id;
    return result;
}

SandboxGroundBlockerDefinition blocker_definition(ContentId id) {
    SandboxGroundBlockerDefinition result{};
    result.id = id;
    return result;
}

SandboxSafePointDefinition safe_point_definition(ContentId id) {
    SandboxSafePointDefinition result{};
    result.id = id;
    return result;
}

struct Fixture final {
    std::array<SandboxInteractionDefinition, 2> interactions{
        interaction_definition(interaction_one),
        interaction_definition(interaction_two),
    };
    std::array<SandboxMechanismDefinition, 2> mechanisms{
        mechanism_definition(mechanism_one),
        mechanism_definition(mechanism_two),
    };
    std::array<SandboxGroundBlockerDefinition, 2> blockers{
        blocker_definition(blocker_one),
        blocker_definition(blocker_two),
    };
    std::array<SandboxSafePointDefinition, 1> safe_points{
        safe_point_definition(safe_point_one),
    };
    std::array<SandboxInteractionGameplayBinding, 2> interaction_bindings{
        SandboxInteractionGameplayBinding{
            interaction_one,
            SandboxInteractionOperation::operate,
            500,
            mechanism_one,
        },
        SandboxInteractionGameplayBinding{
            interaction_two,
            SandboxInteractionOperation::operate,
            3'000,
            mechanism_two,
        },
    };
    std::array<SandboxMechanismGameplayBinding, 2> mechanism_bindings{
        SandboxMechanismGameplayBinding{
            mechanism_one,
            SandboxMechanismActivation::one_shot_activate,
            blocker_one,
        },
        SandboxMechanismGameplayBinding{
            mechanism_two,
            SandboxMechanismActivation::one_shot_activate,
            blocker_two,
        },
    };

    [[nodiscard]] SandboxDefinition core() const noexcept {
        SandboxDefinition result{};
        result.interactions = interactions;
        result.mechanisms = mechanisms;
        result.ground_blockers = blockers;
        result.safe_points = safe_points;
        return result;
    }

    [[nodiscard]] SandboxGameplayBindingDefinition binding() const noexcept {
        return {interaction_bindings, mechanism_bindings};
    }
};

int failures = 0;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        ++failures;
        std::cerr << "sandbox gameplay binding failure: " << message << '\n';
    }
}

void expect_error(
    SandboxGameplayBindingValidationResult result,
    SandboxGameplayBindingValidationCode code,
    SandboxGameplayBindingValidationDomain domain,
    SandboxGameplayBindingValidationField field,
    std::uint32_t record_index,
    std::string_view message
) {
    expect(
        result.code == code && result.domain == domain && result.field == field &&
            result.record_index == record_index,
        message
    );
}

void test_range_contract() {
    expect(
        sandbox_check_operate_range({0, 0, -50'000, 2}, {300, 400, 50'000, 2}, 500) ==
            SandboxOperateRangeCheck::eligible,
        "500 mm is inclusive and height is ignored"
    );
    expect(
        sandbox_check_operate_range({0, 0, 0, 2}, {3'000, 0, 0, 2}, 3'000) ==
            SandboxOperateRangeCheck::eligible,
        "3000 mm is inclusive"
    );
    expect(
        sandbox_check_operate_range({0, 0, 0, 2}, {3'001, 0, 0, 2}, 3'000) ==
            SandboxOperateRangeCheck::out_of_range,
        "distance at range plus one is rejected"
    );
    expect(
        sandbox_check_operate_range({0, 0, 0, 2}, {0, 0, 0, 3}, 500) ==
            SandboxOperateRangeCheck::floor_mismatch,
        "different floor layers are rejected"
    );
    expect(
        sandbox_check_operate_range({0, 0, 0, 2}, {0, 0, 0, 2}, 499) ==
            SandboxOperateRangeCheck::invalid_range &&
            sandbox_check_operate_range({0, 0, 0, 2}, {0, 0, 0, 2}, 3'001) ==
                SandboxOperateRangeCheck::invalid_range,
        "499 and 3001 mm authored ranges are rejected"
    );
    expect(
        sandbox_check_operate_range(
            {std::numeric_limits<std::int32_t>::min(), 0, 0, 0},
            {std::numeric_limits<std::int32_t>::max(), 0, 0, 0},
            3'000
        ) == SandboxOperateRangeCheck::out_of_range,
        "extreme int32 coordinates fail before squaring"
    );
}

void test_valid_and_empty_bindings() {
    const SandboxDefinition empty_core{};
    const SandboxGameplayBindingDefinition empty_binding{};
    const auto empty_result = validate_sandbox_gameplay_binding(empty_core, empty_binding);
    expect(
        sandbox_gameplay_binding_is_valid(empty_result) &&
            empty_result.domain == SandboxGameplayBindingValidationDomain::invalid &&
            empty_result.field == SandboxGameplayBindingValidationField::invalid,
        "empty core and empty binding are valid"
    );

    const Fixture fixture;
    const auto first = validate_sandbox_gameplay_binding(fixture.core(), fixture.binding());
    const auto second = validate_sandbox_gameplay_binding(fixture.core(), fixture.binding());
    expect(
        sandbox_gameplay_binding_is_valid(first) && first == second,
        "a complete typed chain validates deterministically"
    );
}

void test_interaction_binding_failures() {
    using Code = SandboxGameplayBindingValidationCode;
    using Domain = SandboxGameplayBindingValidationDomain;
    using Field = SandboxGameplayBindingValidationField;

    {
        Fixture fixture;
        fixture.interaction_bindings[0].interaction_id = {};
        expect_error(
            validate_sandbox_gameplay_binding(fixture.core(), fixture.binding()),
            Code::invalid_interaction_id,
            Domain::interaction_binding,
            Field::interaction_id,
            0,
            "zero interaction ID fails closed"
        );
    }
    {
        Fixture fixture;
        fixture.interaction_bindings[0].interaction_id = extra_id;
        expect_error(
            validate_sandbox_gameplay_binding(fixture.core(), fixture.binding()),
            Code::dangling_interaction_id,
            Domain::interaction_binding,
            Field::interaction_id,
            0,
            "extra interaction binding fails as dangling"
        );
    }
    {
        Fixture fixture;
        fixture.interaction_bindings[1].interaction_id = interaction_one;
        expect_error(
            validate_sandbox_gameplay_binding(fixture.core(), fixture.binding()),
            Code::duplicate_interaction_binding,
            Domain::interaction_binding,
            Field::interaction_id,
            1,
            "duplicate interaction binding fails"
        );
    }
    for (const auto raw : {0U, 255U}) {
        Fixture fixture;
        fixture.interaction_bindings[0].operation =
            static_cast<SandboxInteractionOperation>(raw);
        expect_error(
            validate_sandbox_gameplay_binding(fixture.core(), fixture.binding()),
            Code::invalid_interaction_operation,
            Domain::interaction_binding,
            Field::operation,
            0,
            "raw 0/255 interaction operation fails"
        );
    }
    for (const auto range : {0, 499, 3'001}) {
        Fixture fixture;
        fixture.interaction_bindings[0].range_mm = range;
        expect_error(
            validate_sandbox_gameplay_binding(fixture.core(), fixture.binding()),
            Code::invalid_operate_range,
            Domain::interaction_binding,
            Field::range_mm,
            0,
            "out-of-contract authored range fails"
        );
    }
    {
        Fixture fixture;
        fixture.interaction_bindings[0].target_mechanism_id = {};
        expect_error(
            validate_sandbox_gameplay_binding(fixture.core(), fixture.binding()),
            Code::invalid_target_mechanism_id,
            Domain::interaction_binding,
            Field::target_mechanism_id,
            0,
            "zero mechanism target fails"
        );
    }
    {
        Fixture fixture;
        fixture.interaction_bindings[1].target_mechanism_id = mechanism_one;
        const auto result = validate_sandbox_gameplay_binding(fixture.core(), fixture.binding());
        expect_error(
            result,
            Code::duplicate_target_mechanism_writer,
            Domain::interaction_binding,
            Field::target_mechanism_id,
            1,
            "each mechanism has exactly one incoming authored interaction"
        );
        expect(
            result.subject == interaction_two.key && result.related == mechanism_one.key,
            "duplicate incoming interaction locates writer and mechanism"
        );
    }
    {
        Fixture fixture;
        fixture.interaction_bindings[0].target_mechanism_id = safe_point_one;
        const auto first = validate_sandbox_gameplay_binding(fixture.core(), fixture.binding());
        const auto second = validate_sandbox_gameplay_binding(fixture.core(), fixture.binding());
        expect_error(
            first,
            Code::dangling_target_mechanism_id,
            Domain::interaction_binding,
            Field::target_mechanism_id,
            0,
            "a Stable ID present only in safe_points is not a mechanism target"
        );
        expect(
            first == second && first.subject == interaction_one.key &&
                first.related == safe_point_one.key,
            "failed validation is deterministic and locates subject and related IDs"
        );
    }
    {
        Fixture fixture;
        const auto interaction_snapshot = fixture.interaction_bindings;
        const auto mechanism_snapshot = fixture.mechanism_bindings;
        const SandboxGameplayBindingDefinition partial{
            std::span<const SandboxInteractionGameplayBinding>{fixture.interaction_bindings}.first(1),
            fixture.mechanism_bindings,
        };
        const auto first = validate_sandbox_gameplay_binding(fixture.core(), partial);
        const auto second = validate_sandbox_gameplay_binding(fixture.core(), partial);
        expect_error(
            first,
            Code::missing_interaction_binding,
            Domain::core_interaction,
            Field::interaction_id,
            1,
            "every non-empty core interaction requires one binding"
        );
        expect(
            first == second && fixture.interaction_bindings == interaction_snapshot &&
                fixture.mechanism_bindings == mechanism_snapshot,
            "failed repeated validation does not mutate input"
        );
    }
}

void test_mechanism_binding_failures() {
    using Code = SandboxGameplayBindingValidationCode;
    using Domain = SandboxGameplayBindingValidationDomain;
    using Field = SandboxGameplayBindingValidationField;

    {
        Fixture fixture;
        fixture.mechanism_bindings[0].mechanism_id = {};
        expect_error(
            validate_sandbox_gameplay_binding(fixture.core(), fixture.binding()),
            Code::invalid_mechanism_id,
            Domain::mechanism_binding,
            Field::mechanism_id,
            0,
            "zero mechanism ID fails"
        );
    }
    {
        Fixture fixture;
        fixture.mechanism_bindings[0].mechanism_id = extra_id;
        expect_error(
            validate_sandbox_gameplay_binding(fixture.core(), fixture.binding()),
            Code::dangling_mechanism_id,
            Domain::mechanism_binding,
            Field::mechanism_id,
            0,
            "extra mechanism binding fails as dangling"
        );
    }
    {
        Fixture fixture;
        fixture.mechanism_bindings[1].mechanism_id = mechanism_one;
        expect_error(
            validate_sandbox_gameplay_binding(fixture.core(), fixture.binding()),
            Code::duplicate_mechanism_binding,
            Domain::mechanism_binding,
            Field::mechanism_id,
            1,
            "duplicate mechanism binding fails"
        );
    }
    for (const auto raw : {0U, 255U}) {
        Fixture fixture;
        fixture.mechanism_bindings[0].activation =
            static_cast<SandboxMechanismActivation>(raw);
        expect_error(
            validate_sandbox_gameplay_binding(fixture.core(), fixture.binding()),
            Code::invalid_mechanism_activation,
            Domain::mechanism_binding,
            Field::activation,
            0,
            "raw 0/255 mechanism activation fails"
        );
    }
    {
        Fixture fixture;
        fixture.mechanism_bindings[0].target_ground_blocker_id = {};
        expect_error(
            validate_sandbox_gameplay_binding(fixture.core(), fixture.binding()),
            Code::invalid_target_ground_blocker_id,
            Domain::mechanism_binding,
            Field::target_ground_blocker_id,
            0,
            "zero blocker target fails"
        );
    }
    {
        Fixture fixture;
        fixture.mechanism_bindings[0].target_ground_blocker_id = interaction_one;
        expect_error(
            validate_sandbox_gameplay_binding(fixture.core(), fixture.binding()),
            Code::dangling_target_ground_blocker_id,
            Domain::mechanism_binding,
            Field::target_ground_blocker_id,
            0,
            "a Stable ID present only in interactions is not a blocker target"
        );
    }
    {
        Fixture fixture;
        fixture.mechanism_bindings[1].target_ground_blocker_id = blocker_one;
        const auto result = validate_sandbox_gameplay_binding(fixture.core(), fixture.binding());
        expect_error(
            result,
            Code::duplicate_ground_blocker_writer,
            Domain::mechanism_binding,
            Field::target_ground_blocker_id,
            1,
            "a blocker has at most one mechanism writer"
        );
        expect(
            result.subject == mechanism_two.key && result.related == blocker_one.key,
            "writer conflict diagnostic retains mechanism and blocker IDs"
        );
    }
    {
        Fixture fixture;
        const SandboxGameplayBindingDefinition partial{
            fixture.interaction_bindings,
            std::span<const SandboxMechanismGameplayBinding>{fixture.mechanism_bindings}.first(1),
        };
        expect_error(
            validate_sandbox_gameplay_binding(fixture.core(), partial),
            Code::missing_mechanism_binding,
            Domain::core_mechanism,
            Field::mechanism_id,
            1,
            "every non-empty core mechanism requires one binding"
        );
    }
    {
        Fixture fixture;
        auto core = fixture.core();
        core.interactions =
            std::span<const SandboxInteractionDefinition>{fixture.interactions}.first(1);
        const SandboxGameplayBindingDefinition disconnected{
            std::span<const SandboxInteractionGameplayBinding>{fixture.interaction_bindings}.first(1),
            fixture.mechanism_bindings,
        };
        expect_error(
            validate_sandbox_gameplay_binding(core, disconnected),
            Code::unreferenced_mechanism,
            Domain::core_mechanism,
            Field::mechanism_id,
            1,
            "a bound mechanism without an incoming interaction is rejected"
        );
    }
}

void test_capacity_failures() {
    using Code = SandboxGameplayBindingValidationCode;
    using Domain = SandboxGameplayBindingValidationDomain;
    using Field = SandboxGameplayBindingValidationField;

    const SandboxDefinition empty_core{};
    std::array<SandboxInteractionGameplayBinding, sandbox_interaction_capacity + 1>
        too_many_interactions{};
    const SandboxGameplayBindingDefinition interaction_overflow{too_many_interactions, {}};
    expect_error(
        validate_sandbox_gameplay_binding(empty_core, interaction_overflow),
        Code::too_many_interaction_bindings,
        Domain::interaction_binding,
        Field::interaction_id,
        static_cast<std::uint32_t>(too_many_interactions.size()),
        "interaction binding capacity is bounded"
    );

    std::array<SandboxMechanismGameplayBinding, sandbox_mechanism_capacity + 1>
        too_many_mechanisms{};
    const SandboxGameplayBindingDefinition mechanism_overflow{{}, too_many_mechanisms};
    expect_error(
        validate_sandbox_gameplay_binding(empty_core, mechanism_overflow),
        Code::too_many_mechanism_bindings,
        Domain::mechanism_binding,
        Field::mechanism_id,
        static_cast<std::uint32_t>(too_many_mechanisms.size()),
        "mechanism binding capacity is bounded"
    );
}

}  // namespace

int main() {
    test_range_contract();
    test_valid_and_empty_bindings();
    test_interaction_binding_failures();
    test_mechanism_binding_failures();
    test_capacity_failures();

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
