#pragma once

#include "application/usecase/settings/LoadSettingsUseCase.h"
#include "application/usecase/settings/SaveSettingUseCase.h"
#include "application/usecase/settings/GetModelsUseCase.h"

namespace application::usecase::settings {
    /**
     * @brief 设置界面业务用例聚合容器
     */
    struct SettingsUseCases {
        LoadSettingsUseCase *loadSettings = nullptr;
        SaveSettingUseCase *saveSetting = nullptr;
        GetModelsUseCase *getModels = nullptr;
    };
} // namespace application::usecase::settings
