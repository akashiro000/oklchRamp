# Copy the freshly built plug-in into the module folder. If Maya still has the
# old .mll loaded, Windows refuses to overwrite it but allows a rename, so move
# the old file aside first (it is cleaned up on the next successful deploy).
if(NOT SRC OR NOT DST)
    message(FATAL_ERROR "deploy.cmake needs -DSRC=<built .mll> -DDST=<target .mll>")
endif()
get_filename_component(DST_DIR "${DST}" DIRECTORY)
file(MAKE_DIRECTORY "${DST_DIR}")
set(OLD "${DST}.old")
if(EXISTS "${OLD}")
    file(REMOVE "${OLD}")   # silently fails if still locked; retried next time
endif()
if(EXISTS "${DST}")
    file(RENAME "${DST}" "${OLD}")
endif()
file(COPY "${SRC}" DESTINATION "${DST_DIR}")
message(STATUS "Deployed ${SRC} -> ${DST}")
