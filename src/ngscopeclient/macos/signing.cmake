if(NOT SIGNING_IDENTITY)
	set(SIGNING_IDENTITY -)
endif()

get_filename_component(APP_BUNDLE "${CMAKE_INSTALL_PREFIX}/../.." ABSOLUTE)

message(STATUS "Signing all dylibs")
execute_process(COMMAND find "${APP_BUNDLE}" -type f -name "*.dylib" -exec codesign --force -s "${SIGNING_IDENTITY}" {} \;)

# The .app bundle isn't fully constructed at this stage, so can't run this here...
#message(STATUS "Signing ngscopeclient.app")
#execute_process(COMMAND codesign --force -s "${SIGNING_IDENTITY}" "${APP_BUNDLE}")

