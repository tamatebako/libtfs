/*
 * Pure C implementation to avoid C++ namespace lookup issues with struct dirent
 * This file handles all struct dirent manipulation in C context
 *
 * IMPORTANT: This is a pure C file with NO project headers
 */

#ifndef _WIN32
// struct dirent exists only on POSIX; the function body is a no-op on Windows
#include <dirent.h>
#endif
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef IFTODT
#define IFTODT(mode) (((mode)&0170000) >> 12)
#endif

void populate_dirent_buffer_c(void* buffer,
                              ino_t ino,
                              off_t offset,
                              mode_t mode,
                              const char* name,
                              size_t name_len,
                              size_t reclen)
{
#ifndef _WIN32
  struct dirent* d = (struct dirent*)buffer;
  d->d_ino = ino;
#ifdef __MACH__
  d->d_seekoff = offset;
#else
  d->d_off = offset;
#endif
  d->d_type = IFTODT(mode);
  strncpy(d->d_name, name, name_len);
  d->d_name[name_len] = '\0';
  d->d_reclen = reclen;
#endif
}
