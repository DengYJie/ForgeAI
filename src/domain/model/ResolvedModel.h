#pragma once
#include <QString>
#include <optional>
#include "CanonicalModel.h"
#include "ProviderModel.h"
#include "ModelProvider.h"

namespace domain::model {

    /**
     * @brief 运行时聚合模型视图
     */
    struct ResolvedModel {
        ModelProvider provider;
        ProviderModel binding;
        std::optional<CanonicalModel> canonical;

        QString requestModelId() const { return binding.remoteModelId; }

        QString displayName() const {
            if (canonical && !canonical->name.isEmpty()) {
                return canonical->name;
            }
            return binding.remoteModelId;
        }

        QString family() const {
            if (canonical && !canonical->family.isEmpty()) {
                return canonical->family;
            }
            return QString();
        }

        QString description() const {
            if (canonical && !canonical->description.isEmpty()) {
                return canonical->description;
            }
            return QString();
        }

        ModelLimit effectiveLimits() const {
            if (binding.limitsOverride.has_value()) {
                return *binding.limitsOverride;
            }
            if (canonical.has_value()) {
                return canonical->limits;
            }
            return ModelLimit{};
        }

        ModelCapabilities effectiveCapabilities() const {
            if (binding.capabilitiesOverride.has_value()) {
                return *binding.capabilitiesOverride;
            }
            if (canonical.has_value()) {
                return canonical->capabilities;
            }
            return ModelCapabilities{};
        }

        const ModelPricing &pricing() const { return binding.pricing; }
        const QString &reasoningField() const { return binding.reasoningField; }
        const QString &group() const { return binding.group; }
        bool isEnabled() const { return provider.isEnabled && binding.isEnabled; }
        bool isCustom() const { return binding.isCustom; }
        DataOrigin origin() const { return binding.origin; }

        bool operator==(const ResolvedModel &other) const = default;
    };

} // namespace domain::model
