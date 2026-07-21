/**
 * @file test_tpkg_c99.c
 * @brief Strict-C99 translation unit proving tebako/tpkg.h compiles as C99.
 *
 * Built with C_STANDARD 99 / C_EXTENSIONS OFF (see CMakeLists). Exercised
 * from test_tpkg.cpp via tpkg_c99_smoke().
 */

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ < 199901L)
#error "tpkg.h must compile as C99 or later"
#endif

#define TPKG_IMPLEMENTATION
#include <tebako/tpkg.h>

#include <string.h>

int tpkg_c99_smoke(void)
{
  tpkg_manifest m;
  uint32_t crc;

  memset(&m, 0, sizeof m);
  m.version = TPKG_VERSION;
  m.package_flags = TPKG_FLAG_LEAN;
  m.slot_count = 1;
  m.launcher_abi = 1;
  strcpy(m.runtime_ref, "ruby@3.3.7;tebako=0.15.0");
  m.slots[0].offset = 0;
  m.slots[0].size = 4096;
  m.slots[0].format_id = TPKG_FORMAT_DWARFS;
  m.slots[0].flags = 0;
  strcpy(m.slots[0].mount_point, "/app");

  crc = tpkg_crc32("123456789", 9);
  if (crc != 0xCBF43926u) {
    return 2;
  }
  if (tpkg_validate(&m) != 0) {
    return 3;
  }
  if (tpkg_errno() != TPKG_OK) {
    return 4;
  }
  if (tpkg_strerror(TPKG_ERR_NO_TRAILER) == NULL) {
    return 5;
  }
  return 0;
}
