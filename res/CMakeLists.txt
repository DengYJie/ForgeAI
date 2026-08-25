set(MODELS_DEV_API_URL "https://models.dev/api.json")
set(API_JSON_TARGET "${CMAKE_CURRENT_SOURCE_DIR}/config/api.json")

set(MODELS_DEV_MODELS_URL "https://models.dev/models.json")
set(MODELS_JSON_TARGET "${CMAKE_CURRENT_SOURCE_DIR}/config/models.json")

# Metadata refresh is an explicit opt-in.  Configuring a project should not
# require network access when the checked-in cache is sufficient.
if (FORGEAI_UPDATE_MODELS)
    message(STATUS "正在从 ${MODELS_DEV_API_URL} 同步 Provider 元数据...")
    file(DOWNLOAD
            "${MODELS_DEV_API_URL}"
            "${API_JSON_TARGET}"
            STATUS API_DOWNLOAD_STATUS
            TIMEOUT 15
            SHOW_PROGRESS
    )
    list(GET API_DOWNLOAD_STATUS 0 API_STATUS_CODE)
    list(GET API_DOWNLOAD_STATUS 1 API_ERROR_MESSAGE)
else ()
    set(API_STATUS_CODE 1)
    set(API_ERROR_MESSAGE "未启用 FORGEAI_UPDATE_MODELS")
endif ()

if (NOT FORGEAI_UPDATE_MODELS)
    message(STATUS "使用本地 api.json 缓存（FORGEAI_UPDATE_MODELS=OFF）")
elseif (NOT API_STATUS_CODE EQUAL 0)
    if (EXISTS "${API_JSON_TARGET}")
        message(WARNING "从 models.dev 同步 api.json 失败 (${API_ERROR_MESSAGE})，将使用本地现有的 api.json 缓存。")
    elseif (EXISTS "${MODELS_JSON_TARGET}")
        # 兼容过渡：若已有旧 models.json，拷贝一份为 api.json
        file(COPY_FILE "${MODELS_JSON_TARGET}" "${API_JSON_TARGET}")
        message(STATUS "已从旧缓存生成: ${API_JSON_TARGET}")
    else ()
        message(FATAL_ERROR "从 models.dev 同步 api.json 失败且无本地缓存: ${API_ERROR_MESSAGE}")
    endif ()
else ()
    message(STATUS "成功同步 Provider 元数据至: ${API_JSON_TARGET}")
endif ()

# 2. 同步模型本体元数据 (models.json)
if (FORGEAI_UPDATE_MODELS)
    message(STATUS "正在从 ${MODELS_DEV_MODELS_URL} 同步模型本体元数据...")
    file(DOWNLOAD
            "${MODELS_DEV_MODELS_URL}"
            "${MODELS_JSON_TARGET}"
            STATUS MODELS_DOWNLOAD_STATUS
            TIMEOUT 15
            SHOW_PROGRESS
    )
    list(GET MODELS_DOWNLOAD_STATUS 0 MODELS_STATUS_CODE)
    list(GET MODELS_DOWNLOAD_STATUS 1 MODELS_ERROR_MESSAGE)
else ()
    set(MODELS_STATUS_CODE 1)
    set(MODELS_ERROR_MESSAGE "未启用 FORGEAI_UPDATE_MODELS")
endif ()

if (NOT FORGEAI_UPDATE_MODELS)
    message(STATUS "使用本地 models.json 缓存（FORGEAI_UPDATE_MODELS=OFF）")
elseif (NOT MODELS_STATUS_CODE EQUAL 0)
    if (EXISTS "${MODELS_JSON_TARGET}")
        message(WARNING "从 models.dev 同步 models.json 失败 (${MODELS_ERROR_MESSAGE})，将使用本地现有的 models.json 缓存。")
    else ()
        message(WARNING "从 models.dev 同步 models.json 失败: ${MODELS_ERROR_MESSAGE}")
    endif ()
else ()
    message(STATUS "成功同步模型本体元数据至: ${MODELS_JSON_TARGET}")
endif ()

qt_add_resources(${PROJECT_NAME} "app_resources"
        PREFIX "/"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}"
        FILES
        config/api.json
        config/models.json
)
