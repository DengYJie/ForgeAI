#pragma once

#include "qlitehtml_global.h"

#include <QByteArray>
#include <QUrl>
#include <functional>

namespace qlitehtml {

/**
 * @brief Identifies the type of an external HTML/CSS resource.
 */
enum class ResourceType {
    Image,       ///< Raster/vector image resource (<img>, CSS background-image, list markers).
    StyleSheet,  ///< External stylesheet resource (<link rel="stylesheet">, CSS @import).
    Font         ///< Web font resource (CSS @font-face).
};

/**
 * @brief Custom handler callback for intercepting and serving resource requests.
 * @note Must be thread-safe and re-entrant, as it may be invoked concurrently from worker threads.
 * @param url The resolved target URL.
 * @param type The type of the requested resource.
 * @return The raw binary payload of the resource, or an empty QByteArray to fall through to default fetchers.
 */
using ResourceHandler = std::function<QByteArray(const QUrl &url, ResourceType type)>;

/**
 * @brief Repaint notification callback invoked on the GUI thread when an asynchronous resource finishes loading.
 */
using RepaintCallback = std::function<void()>;

} // namespace qlitehtml
