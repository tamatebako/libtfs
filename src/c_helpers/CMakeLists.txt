# Pure C helper library with NO project includes
# Only system includes are used

add_library(tebako_dirent_helper_c STATIC
  tebako-dirent-helper.c
)

set_target_properties(tebako_dirent_helper_c PROPERTIES
  LANGUAGE C
  POSITION_INDEPENDENT_CODE ON
)

# Install and export the C helper library
install(TARGETS tebako_dirent_helper_c
  EXPORT libtfsTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
