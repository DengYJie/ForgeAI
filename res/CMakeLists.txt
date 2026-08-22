set(MODELS_DEV_API_URL "https://models.dev/api.json")
set(MODELS_JSON_TARGET "${CMAKE_CURRENT_SOURCE_DIR}/config/models.json")

message(STATUS "正在从 ${MODELS_DEV_API_URL} 同步最新模型元数据目录...")
file(DOWNLOAD
        "${MODELS_DEV_API_URL}"
        "${MODELS_JSON_TARGET}"
        STATUS DOWNLOAD_STATUS
        TIMEOUT 15
        SHOW_PROGRESS
)
list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
list(GET DOWNLOAD_STATUS 1 ERROR_MESSAGE)

if (NOT STATUS_CODE EQUAL 0)
    if (EXISTS "${MODELS_JSON_TARGET}")
        message(WARNING "从 models.dev 自动同步失败 (${ERROR_MESSAGE})，将使用本地现有的模型目录缓存。")
    else ()
        message(FATAL_ERROR "从 models.dev 自动拉取模型目录失败且无本地缓存: ${ERROR_MESSAGE}")
    endif ()
else ()
    message(STATUS "成功同步全量模型目录至: ${MODELS_JSON_TARGET}")
endif ()

qt_add_resources(${PROJECT_NAME} "app_resources"
        PREFIX "/"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}"
        FILES
        config/models.json
)
