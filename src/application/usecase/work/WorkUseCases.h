#pragma once

#include "application/usecase/work/StartTaskUseCase.h"
#include "application/usecase/work/CancelTaskUseCase.h"

namespace application::usecase::work {
    /**
     * @brief 工作流界面业务用例聚合容器
     */
    struct WorkUseCases {
        StartTaskUseCase *startTask = nullptr;
        CancelTaskUseCase *cancelTask = nullptr;
    };
} // namespace application::usecase::work
