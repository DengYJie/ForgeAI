set(API_JSON_TARGET "${CMAKE_CURRENT_SOURCE_DIR}/config/api.json")
set(MODELS_JSON_TARGET "${CMAKE_CURRENT_SOURCE_DIR}/config/models.json")

# Refreshing external model metadata is a deliberate operation, never an
# implicit side effect of configure.  Run:
#   cmake --build --preset debug --target update-model-catalog
add_custom_target(update-model-catalog
    COMMAND "${CMAKE_COMMAND}"
        -DAPI_JSON_TARGET="${API_JSON_TARGET}"
        -DMODELS_JSON_TARGET="${MODELS_JSON_TARGET}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/UpdateModelCatalog.cmake"
    USES_TERMINAL
    COMMENT "Refreshing models.dev metadata"
)

qt_add_resources(${PROJECT_NAME} "app_resources"
        PREFIX "/"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}"
        FILES
        config/api.json
        config/models.json
)
