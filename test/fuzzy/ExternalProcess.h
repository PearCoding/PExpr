#pragma once

#include "PExpr.h"

#include <vector>

namespace PExpr {
class ExternalProcess {
public:
    ExternalProcess(const std::filesystem::path& exe, const std::vector<std::string>& parameters);
    ~ExternalProcess();

    [[nodiscard]] int run();

    [[nodiscard]] int exitCode() const;

private:
    std::unique_ptr<class ExternalProcessInternal> mInternal;
};
} // namespace PExpr