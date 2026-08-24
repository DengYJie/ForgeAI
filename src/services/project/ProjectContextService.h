#pragma once
#include "domain/service/IProjectContextService.h"
namespace services::project {
class ProjectContextService final : public domain::service::IProjectContextService {
public:
    domain::project::ProjectContext load(const QString& rootPath) const override;
};
}
