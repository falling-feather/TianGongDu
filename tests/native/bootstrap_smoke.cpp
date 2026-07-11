#include <tgd/contracts/build_identity.hpp>
#include <tgd/presentation/presentation_lifecycle.hpp>
#include <tgd/runtime/runtime_facade.hpp>

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "bootstrap smoke failure: " << message << '\n';
    }
    return condition;
}

}  // namespace

int main() {
    using tgd::presentation::PresentationError;
    using tgd::presentation::PresentationLifecycle;
    using tgd::presentation::PresentationState;
    using tgd::runtime::RuntimeError;
    using tgd::runtime::RuntimeFacade;
    using tgd::runtime::RuntimeLifecycle;

    bool ok = true;
    const auto identity = tgd::contracts::current_build_identity();
    ok &= expect(!identity.semantic_version.empty(), "semantic version is embedded");
    ok &= expect(!identity.git_commit.empty(), "git commit is embedded");
    ok &= expect(identity.channel == "prototype_f1", "F1 channel is isolated");

    RuntimeFacade runtime;
    PresentationLifecycle presentation;
    ok &= expect(runtime.lifecycle() == RuntimeLifecycle::cold, "runtime begins cold");
    ok &= expect(
        presentation.start(runtime) == PresentationError::runtime_not_ready,
        "presentation cannot own startup before runtime"
    );
    ok &= expect(runtime.initialize() == RuntimeError::none, "runtime initializes");
    ok &= expect(runtime.initialize() == RuntimeError::already_initialized, "duplicate init is explicit");
    ok &= expect(presentation.start(runtime) == PresentationError::none, "presentation starts");
    ok &= expect(presentation.suspend() == PresentationError::none, "presentation suspends");
    ok &= expect(presentation.context_lost() == PresentationError::none, "context loss is represented");
    ok &= expect(
        presentation.context_restored() == PresentationError::none &&
            presentation.state() == PresentationState::suspended,
        "context restore preserves suspended state"
    );
    ok &= expect(presentation.resume() == PresentationError::none, "presentation resumes");
    ok &= expect(presentation.stop() == PresentationError::none, "presentation stops");
    ok &= expect(runtime.shutdown() == RuntimeError::none, "runtime shuts down after presentation");
    ok &= expect(runtime.shutdown() == RuntimeError::not_initialized, "duplicate shutdown is explicit");

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
