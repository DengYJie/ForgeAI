#pragma once

#include <memory>
#include "application/usecase/settings/LoadSettingsUseCase.h"
#include "application/usecase/settings/SaveSettingUseCase.h"
#include "application/usecase/settings/GetSettingsProvidersUseCase.h"
#include "application/usecase/settings/GetModelsUseCase.h"
#include "application/usecase/settings/RefreshModelsUseCase.h"
#include "application/usecase/settings/SaveProviderUseCase.h"
#include "application/usecase/settings/DeleteProviderUseCase.h"

namespace core::model {
    class ModelRegistry;
}

namespace application::usecase::settings {
    /**
     * @brief 设置界面业务用例聚合容器
     */
    struct SettingsUseCases {
        LoadSettingsUseCase *loadSettings = nullptr;
        SaveSettingUseCase *saveSetting = nullptr;
        GetSettingsProvidersUseCase *getSettingsProviders = nullptr;
        GetModelsUseCase *getModels = nullptr;
        RefreshModelsUseCase *refreshModels = nullptr;
        SaveProviderUseCase *saveProvider = nullptr;
        DeleteProviderUseCase *deleteProvider = nullptr;
        std::shared_ptr<core::model::ModelRegistry> modelRegistry = nullptr;
    };
} // namespace application::usecase::settings
