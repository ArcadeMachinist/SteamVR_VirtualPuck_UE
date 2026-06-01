# Copies SRC to DST only when DST does not already exist.
# Preserves user edits made to the staged config.json.
if(NOT EXISTS "${DST}")
    configure_file("${SRC}" "${DST}" COPYONLY)
    message(STATUS "Created default config: ${DST}")
else()
    message(STATUS "Config already exists, skipping: ${DST}")
endif()
