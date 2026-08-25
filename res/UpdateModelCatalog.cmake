cmake_minimum_required(VERSION 3.21)

if (NOT DEFINED API_JSON_TARGET OR NOT DEFINED MODELS_JSON_TARGET)
    set(API_JSON_TARGET "${CMAKE_CURRENT_LIST_DIR}/config/api.json")
    set(MODELS_JSON_TARGET "${CMAKE_CURRENT_LIST_DIR}/config/models.json")
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
        if (EXISTS "${target}")
            file(SIZE "${target}" existing_size)
            if (existing_size GREATER 0)
                message(WARNING "Unable to download ${label}: ${status_message}. Using existing cache.")
                return()
            endif()
        endif()
        message(WARNING "Unable to download ${label}: ${status_message}")
        return()
    endif()

    file(SIZE "${temp_file}" file_size)
    if (file_size EQUAL 0)
        file(REMOVE "${temp_file}")
        if (EXISTS "${target}")
            file(SIZE "${target}" existing_size)
            if (existing_size GREATER 0)
                message(WARNING "Downloaded ${label} is empty. Using existing cache.")
                return()
            endif()
        endif()
        message(WARNING "Downloaded ${label} is empty")
        return()
    endif()

    # Replace the resource only after a complete download succeeds.
    file(COPY_FILE "${temp_file}" "${target}" ONLY_IF_DIFFERENT RESULT copy_result)
    file(REMOVE "${temp_file}")
    if (NOT copy_result STREQUAL "0")
        message(WARNING "Unable to update ${label}: ${copy_result}")
        return()
    endif()
    message(STATUS "Updated ${label}: ${target}")
endfunction()

download_catalog("https://models.dev/api.json" "${API_JSON_TARGET}" "api.json")
download_catalog("https://models.dev/models.json" "${MODELS_JSON_TARGET}" "models.json")
