#pragma once

#include "qlitehtml_global.h"

#include <QByteArray>
#include <QUrl>
#include <functional>

namespace qlitehtml {

enum class ResourceType {
    Image,
    StyleSheet,
    Font
};

using ResourceHandler = std::function<QByteArray(const QUrl &url, ResourceType type)>;
using RepaintCallback = std::function<void()>;

} // namespace qlitehtml
