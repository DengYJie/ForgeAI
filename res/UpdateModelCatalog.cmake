cmake_minimum_required(VERSION 3.21)

if (NOT DEFINED API_JSON_TARGET OR NOT DEFINED MODELS_JSON_TARGET)
    message(FATAL_ERROR "API_JSON_TARGET and MODELS_JSON_TARGET are required")
endif()

function(download_catalog url target label)
    get_filename_component(target_dir "${target}" DIRECTORY)
    file(MAKE_DIRECTORY "${target_dir}")

    set(temp_file "${target}.download")
    file(REMOVE "${temp_file}")
    message(STATUS "Downloading ${label} from ${url}")
    file(DOWNLOAD "${url}" "${temp_file}"
        STATUS status
        TLS_VERIFY ON
        TIMEOUT 30
        SHOW_PROGRESS
    )
    list(GET status 0 status_code)
    list(GET status 1 status_message)
    if (NOT status_code EQUAL 0)
        file(REMOVE "${temp_file}")
        message(FATAL_ERROR "Unable to download ${label}: ${status_message}")
    endif()

    file(SIZE "${temp_file}" file_size)
    if (file_size EQUAL 0)
        file(REMOVE "${temp_file}")
        message(FATAL_ERROR "Downloaded ${label} is empty")
    endif()

    # Replace the resource only after a complete download succeeds.
    file(COPY_FILE "${temp_file}" "${target}" ONLY_IF_DIFFERENT RESULT copy_result)
    file(REMOVE "${temp_file}")
    if (NOT copy_result STREQUAL "0")
        message(FATAL_ERROR "Unable to update ${label}: ${copy_result}")
    endif()
    message(STATUS "Updated ${label}: ${target}")
endfunction()

download_catalog("https://models.dev/api.json" "${API_JSON_TARGET}" "api.json")
download_catalog("https://models.dev/models.json" "${MODELS_JSON_TARGET}" "models.json")
