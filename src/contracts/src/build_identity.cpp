#include <tgd/contracts/build_identity.hpp>

#include <tgd/contracts/build_config.hpp>

namespace tgd::contracts {

BuildIdentity current_build_identity() noexcept {
    return {
        .semantic_version = build::semantic_version,
        .git_commit = build::git_commit,
        .channel = build::channel,
    };
}

}  // namespace tgd::contracts
