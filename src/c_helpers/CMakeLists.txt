# Pure C helper library with NO project includes
# Only system includes are used

if(WITH_LEGACY_TEBAKO_API)
  add_library(tebako_dirent_helper_c STATIC
    tebako-dirent-helper.c
  )

  set_target_properties(tebako_dirent_helper_c PROPERTIES
    LANGUAGE C
    POSITION_INDEPENDENT_CODE ON
  )
else()
  # The only consumer of the helper is the legacy tebako API (tebako-dirent.cpp).
  # When the legacy layer is off (standalone MinGW) nothing needs it, but tfs
  # and the test executables link tebako_dirent_helper_c unconditionally, so
  # provide an empty interface target to keep those link lines unchanged.
  add_library(tebako_dirent_helper_c INTERFACE)
endif()

# Install and export the C helper library
install(TARGETS tebako_dirent_helper_c
  EXPORT libtfsTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
