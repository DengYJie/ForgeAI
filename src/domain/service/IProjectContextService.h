#pragma once
#include "domain/project/ProjectContext.h"
namespace domain::service {
class IProjectContextService {
public:
    virtual ~IProjectContextService() = default;
    virtual domain::project::ProjectContext load(const QString& rootPath) const = 0;
};
}
