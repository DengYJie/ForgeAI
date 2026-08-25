#pragma once

#include <QList>
#include <memory>
#include "application/ports/ITool.h"

namespace application::ports {

    /**
     * @brief 工具提供者接口（如 BuiltinToolProvider, McpToolProvider）
     */
    class IToolProvider {
    public:
        virtual ~IToolProvider() = default;

        /**
         * @brief 获取该 Provider 提供的所有工具实例列表
         */
        virtual QList<std::shared_ptr<ITool>> tools() const = 0;
    };

} // namespace application::ports
