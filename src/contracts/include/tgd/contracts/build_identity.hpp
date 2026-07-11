#pragma once

#include <string_view>

namespace tgd::contracts {

struct BuildIdentity final {
    std::string_view semantic_version;
    std::string_view git_commit;
    std::string_view channel;
};

[[nodiscard]] BuildIdentity current_build_identity() noexcept;

}  // namespace tgd::contracts
