/**
 *
 * Copyright (c) 2024, [Ribose Inc](https://www.ribose.com).
 * All rights reserved.
 * This file is a part of the Tebako project. (libdwarfs-wr)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other matrials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* For the functions marked with: "License: Ruby's"
 *
 * Copyright (C) 1993-2013 Yukihiro Matsumoto. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/* For the functions marked with: "License: Artistic or GPL"
 *
 *    Copyright (c) 1993:: Intergraph Corporation
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the perl README file.
 */

#include <tebako-pch.h>
#include <time.h>
#include <sys/timeb.h>
#include <stdio.h>
#include "tests-defines.h"

#if defined(__MINGW32__) && !defined(min)
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#include <tebako/fs/io.h>
#include <tebako/fs/ruby/io_rb_w32_inner.h>

#if defined(_WIN32) && defined(RB_W32)

#ifndef PATH_MAX
#if defined MAX_PATH
#define PATH_MAX MAX_PATH
#elif defined HAVE_SYS_PARAM_H
#include <sys/param.h>
#define PATH_MAX MAXPATHLEN
#endif
#include <Time.h>
#endif

#if !defined S_IFIFO && defined _S_IFIFO
#define S_IFIFO _S_IFIFO
#endif

#define O_SHARE_DELETE 0x20000000 /* for rb_w32_open(), rb_w32_wopen() */

#define LONG_LONG long long

#ifndef numberof
#define numberof(array) (int)(sizeof(array) / sizeof((array)[0]))
#endif

enum ruby_preserved_encindex {
  RUBY_ENCINDEX_ASCII,
  RUBY_ENCINDEX_UTF_8,
  RUBY_ENCINDEX_US_ASCII,

  /* preserved indexes */
  RUBY_ENCINDEX_UTF_16BE,
  RUBY_ENCINDEX_UTF_16LE,
  RUBY_ENCINDEX_UTF_32BE,
  RUBY_ENCINDEX_UTF_32LE,
  RUBY_ENCINDEX_UTF_16,
  RUBY_ENCINDEX_UTF_32,
  RUBY_ENCINDEX_UTF8_MAC,

  /* for old options of regexp */
  RUBY_ENCINDEX_EUC_JP,
  RUBY_ENCINDEX_Windows_31J,

  RUBY_ENCINDEX_BUILTIN_MAX
};

#define ENCINDEX_ASCII RUBY_ENCINDEX_ASCII
#define ENCINDEX_UTF_8 RUBY_ENCINDEX_UTF_8
#define ENCINDEX_US_ASCII RUBY_ENCINDEX_US_ASCII
#define ENCINDEX_UTF_16BE RUBY_ENCINDEX_UTF_16BE
#define ENCINDEX_UTF_16LE RUBY_ENCINDEX_UTF_16LE
#define ENCINDEX_UTF_32BE RUBY_ENCINDEX_UTF_32BE
#define ENCINDEX_UTF_32LE RUBY_ENCINDEX_UTF_32LE
#define ENCINDEX_UTF_16 RUBY_ENCINDEX_UTF_16
#define ENCINDEX_UTF_32 RUBY_ENCINDEX_UTF_32
#define ENCINDEX_UTF8_MAC RUBY_ENCINDEX_UTF8_MAC
#define ENCINDEX_EUC_JP RUBY_ENCINDEX_EUC_JP
#define ENCINDEX_Windows_31J RUBY_ENCINDEX_Windows_31J
#define ENCINDEX_BUILTIN_MAX RUBY_ENCINDEX_BUILTIN_MAX

static const WCHAR namespace_prefix[] = {L'\\', L'\\', L'?', L'\\'};
enum { FINAL_PATH_MAX = PATH_MAX + numberof(namespace_prefix) };

#define filecp rb_w32_filecp
#define mbstr_to_wstr rb_w32_mbstr_to_wstr
#define wstr_to_mbstr rb_w32_wstr_to_mbstr
#define acp_to_wstr(str, plen) mbstr_to_wstr(CP_ACP, str, -1, plen)
#define wstr_to_acp(str, plen) wstr_to_mbstr(CP_ACP, str, -1, plen)
#define filecp_to_wstr(str, plen) mbstr_to_wstr(filecp(), str, -1, plen)
#define wstr_to_filecp(str, plen) wstr_to_mbstr(filecp(), str, -1, plen)
#define utf8_to_wstr(str, plen) mbstr_to_wstr(CP_UTF8, str, -1, plen)
#define wstr_to_utf8(str, plen) wstr_to_mbstr(CP_UTF8, str, -1, plen)

#define map_errno rb_w32_map_errno

#define ALLOCA(type) ALLOCA_N(type, 1)
#define ALLOCA_N(type, n) ((type*)_alloca((n) * sizeof(type)))

#define VALUE void*
#define RUBY_CRITICAL

#define isdirsep(x) ((x) == '/' || (x) == '\\')

static int check_valid_dir(const WCHAR* path);
static unsigned fileattr_to_unixmode(DWORD attr, const WCHAR* path, unsigned mode);
static time_t filetime_to_unixtime(const FILETIME* ft);
static long filetime_to_nsec(const FILETIME* ft);
static HANDLE open_dir_handle(const WCHAR* filename, WIN32_FIND_DATAW* fd);
static FARPROC get_proc_address(const char* module, const char* func, HANDLE* mh);
static time_t filetime_split(const FILETIME* ft, long* subsec);
static DWORD stati128_handle(HANDLE h, struct stati128* st);
static DWORD get_ino(HANDLE h, FILE_ID_INFO* id);
static UINT filecp(void);
static int w32_wopen(const WCHAR* file, int oflag, int pmode);
int rb_w32_wopen(const WCHAR* file, int oflag, ...);
static int check_if_wdir(const WCHAR* wfile);
char* rb_w32_wstr_to_mbstr(UINT cp, const WCHAR* wstr, int clen, long* plen);
char* rb_w32_conv_from_wstr(const WCHAR* wstr, long* lenp, const void* enc);
static void move_to_next_entry(DIR* dirp);
static int w32_stati128(const char* path, struct stati128* st, UINT cp, BOOL lstat);
static int wstati128(const WCHAR* path, struct stati128* st, BOOL lstat);

typedef char lowio_text_mode;
typedef char lowio_pipe_lookahead[3];

typedef struct {
  CRITICAL_SECTION lock;
  intptr_t osfhnd;       // underlying OS file HANDLE
  __int64 startpos;      // File position that matches buffer start
  unsigned char osfile;  // Attributes of file (e.g., open in text mode?)
  lowio_text_mode textmode;
  lowio_pipe_lookahead _pipe_lookahead;

  uint8_t unicode : 1;           // Was the file opened as unicode?
  uint8_t utf8translations : 1;  // Buffer contains translations other than CRLF
  uint8_t dbcsBufferUsed : 1;    // Is the dbcsBuffer in use?
  char dbcsBuffer;               // Buffer for the lead byte of DBCS when converting from
                                 // DBCS to Unicode
} ioinfo;

static ioinfo** __pioinfo = NULL;
#define IOINFO_L2E 6
static inline ioinfo* _pioinfo(int);

static size_t pioinfo_extra = 0; /* workaround for VC++8 SP1 */

#define IOINFO_ARRAY_ELTS (1 << IOINFO_L2E)

#define _osfhnd(i) (_pioinfo(i)->osfhnd)
#define _osfile(i) (_pioinfo(i)->osfile)

#define _set_osfhnd(fh, osfh) (void)(_osfhnd(fh) = osfh)
#define _set_osflags(fh, flags) (_osfile(fh) = (flags))

/* License: Ruby's */
static void set_pioinfo_extra(void)
{
#define FUNCTION_RET 0xc3 /* ret */
#ifdef _DEBUG
#define UCRTBASE "ucrtbased.dll"
#else
#define UCRTBASE "ucrtbase.dll"
#endif
  /* get __pioinfo addr with _isatty */
  char* p = (char*)get_proc_address(UCRTBASE, "_isatty", NULL);
  char* pend = p;
  /* _osfile(fh) & FDEV */

  int32_t rel;
  char* rip;
  /* add rsp, _ */
#define FUNCTION_BEFORE_RET_MARK "\x48\x83\xc4"
#define FUNCTION_SKIP_BYTES 1
#ifdef _DEBUG
  /* lea rcx,[__pioinfo's addr in RIP-relative 32bit addr] */
#define PIOINFO_MARK "\x48\x8d\x0d"
#else
  /* lea rdx,[__pioinfo's addr in RIP-relative 32bit addr] */
#define PIOINFO_MARK "\x48\x8d\x15"
#endif

  if (p) {
    for (pend += 10; pend < p + 300; pend++) {
      // find end of function
      if (memcmp(pend, FUNCTION_BEFORE_RET_MARK, sizeof(FUNCTION_BEFORE_RET_MARK) - 1) == 0 &&
          (*(pend + (sizeof(FUNCTION_BEFORE_RET_MARK) - 1) + FUNCTION_SKIP_BYTES) & FUNCTION_RET) == FUNCTION_RET) {
        // search backwards from end of function
        for (pend -= (sizeof(PIOINFO_MARK) - 1); pend > p; pend--) {
          if (memcmp(pend, PIOINFO_MARK, sizeof(PIOINFO_MARK) - 1) == 0) {
            p = pend;
            goto found;
          }
        }
        break;
      }
    }
  }
  fprintf(stderr, "unexpected " UCRTBASE "\n");
  _exit(1);

found:
  p += sizeof(PIOINFO_MARK) - 1;
  rel = *(int32_t*)(p);
  rip = p + sizeof(int32_t);
  __pioinfo = (ioinfo**)(rip + rel);

  int fd;

  fd = _open("NUL", O_RDONLY);
  for (pioinfo_extra = 0; pioinfo_extra <= 64; pioinfo_extra += sizeof(void*)) {
    if (_osfhnd(fd) == _get_osfhandle(fd)) {
      break;
    }
  }
  _close(fd);

  if (pioinfo_extra > 64) {
    /* not found, maybe something wrong... */
    pioinfo_extra = 0;
  }
}

static inline ioinfo* _pioinfo(int fd)
{
  if (__pioinfo == NULL) {
    set_pioinfo_extra();
  }
  const size_t sizeof_ioinfo = sizeof(ioinfo) + pioinfo_extra;
  return (ioinfo*)((char*)__pioinfo[fd >> IOINFO_L2E] + (fd & (IOINFO_ARRAY_ELTS - 1)) * sizeof_ioinfo);
}

#define FOPEN 0x01      /* file handle open */
#define FEOFLAG 0x02    /* end of file has been encountered */
#define FPIPE 0x08      /* file handle refers to a pipe */
#define FNOINHERIT 0x10 /* file handle opened O_NOINHERIT */
#define FAPPEND 0x20    /* file handle opened O_APPEND */
#define FDEV 0x40       /* file handle refers to device */
#define FTEXT 0x80      /* file handle is in text mode */

/* errno mapping */
static const struct {
  DWORD winerr;
  int err;
} errmap[] = {
    {ERROR_INVALID_FUNCTION, EINVAL},
    {ERROR_FILE_NOT_FOUND, ENOENT},
    {ERROR_PATH_NOT_FOUND, ENOENT},
    {ERROR_TOO_MANY_OPEN_FILES, EMFILE},
    {ERROR_ACCESS_DENIED, EACCES},
    {ERROR_INVALID_HANDLE, EBADF},
    {ERROR_ARENA_TRASHED, ENOMEM},
    {ERROR_NOT_ENOUGH_MEMORY, ENOMEM},
    {ERROR_INVALID_BLOCK, ENOMEM},
    {ERROR_BAD_ENVIRONMENT, E2BIG},
    {ERROR_BAD_FORMAT, ENOEXEC},
    {ERROR_INVALID_ACCESS, EINVAL},
    {ERROR_INVALID_DATA, EINVAL},
    {ERROR_INVALID_DRIVE, ENOENT},
    {ERROR_CURRENT_DIRECTORY, EACCES},
    {ERROR_NOT_SAME_DEVICE, EXDEV},
    {ERROR_NO_MORE_FILES, ENOENT},
    {ERROR_WRITE_PROTECT, EROFS},
    {ERROR_BAD_UNIT, ENODEV},
    {ERROR_NOT_READY, ENXIO},
    {ERROR_BAD_COMMAND, EACCES},
    {ERROR_CRC, EACCES},
    {ERROR_BAD_LENGTH, EACCES},
    {ERROR_SEEK, EIO},
    {ERROR_NOT_DOS_DISK, EACCES},
    {ERROR_SECTOR_NOT_FOUND, EACCES},
    {ERROR_OUT_OF_PAPER, EACCES},
    {ERROR_WRITE_FAULT, EIO},
    {ERROR_READ_FAULT, EIO},
    {ERROR_GEN_FAILURE, EACCES},
    {ERROR_LOCK_VIOLATION, EACCES},
    {ERROR_SHARING_VIOLATION, EACCES},
    {ERROR_WRONG_DISK, EACCES},
    {ERROR_SHARING_BUFFER_EXCEEDED, EACCES},
    {ERROR_BAD_NETPATH, ENOENT},
    {ERROR_NETWORK_ACCESS_DENIED, EACCES},
    {ERROR_BAD_NET_NAME, ENOENT},
    {ERROR_FILE_EXISTS, EEXIST},
    {ERROR_CANNOT_MAKE, EACCES},
    {ERROR_FAIL_I24, EACCES},
    {ERROR_INVALID_PARAMETER, EINVAL},
    {ERROR_NO_PROC_SLOTS, EAGAIN},
    {ERROR_DRIVE_LOCKED, EACCES},
    {ERROR_BROKEN_PIPE, EPIPE},
    {ERROR_DISK_FULL, ENOSPC},
    {ERROR_INVALID_TARGET_HANDLE, EBADF},
    {ERROR_INVALID_HANDLE, EINVAL},
    {ERROR_WAIT_NO_CHILDREN, ECHILD},
    {ERROR_CHILD_NOT_COMPLETE, ECHILD},
    {ERROR_DIRECT_ACCESS_HANDLE, EBADF},
    {ERROR_NEGATIVE_SEEK, EINVAL},
    {ERROR_SEEK_ON_DEVICE, EACCES},
    {ERROR_DIR_NOT_EMPTY, ENOTEMPTY},
    {ERROR_DIRECTORY, ENOTDIR},
    {ERROR_NOT_LOCKED, EACCES},
    {ERROR_BAD_PATHNAME, ENOENT},
    {ERROR_MAX_THRDS_REACHED, EAGAIN},
    {ERROR_LOCK_FAILED, EACCES},
    {ERROR_ALREADY_EXISTS, EEXIST},
    {ERROR_INVALID_STARTING_CODESEG, ENOEXEC},
    {ERROR_INVALID_STACKSEG, ENOEXEC},
    {ERROR_INVALID_MODULETYPE, ENOEXEC},
    {ERROR_INVALID_EXE_SIGNATURE, ENOEXEC},
    {ERROR_EXE_MARKED_INVALID, ENOEXEC},
    {ERROR_BAD_EXE_FORMAT, ENOEXEC},
    {ERROR_ITERATED_DATA_EXCEEDS_64k, ENOEXEC},
    {ERROR_INVALID_MINALLOCSIZE, ENOEXEC},
    {ERROR_DYNLINK_FROM_INVALID_RING, ENOEXEC},
    {ERROR_IOPL_NOT_ENABLED, ENOEXEC},
    {ERROR_INVALID_SEGDPL, ENOEXEC},
    {ERROR_AUTODATASEG_EXCEEDS_64k, ENOEXEC},
    {ERROR_RING2SEG_MUST_BE_MOVABLE, ENOEXEC},
    {ERROR_RELOC_CHAIN_XEEDS_SEGLIM, ENOEXEC},
    {ERROR_INFLOOP_IN_RELOC_CHAIN, ENOEXEC},
    {ERROR_FILENAME_EXCED_RANGE, ENOENT},
    {ERROR_NESTING_NOT_ALLOWED, EAGAIN},
#ifndef ERROR_PIPE_LOCAL
#define ERROR_PIPE_LOCAL 229L
#endif
    {ERROR_PIPE_LOCAL, EPIPE},
    {ERROR_BAD_PIPE, EPIPE},
    {ERROR_PIPE_BUSY, EAGAIN},
    {ERROR_NO_DATA, EPIPE},
    {ERROR_PIPE_NOT_CONNECTED, EPIPE},
    {ERROR_OPERATION_ABORTED, EINTR},
    {ERROR_NOT_ENOUGH_QUOTA, ENOMEM},
    {ERROR_MOD_NOT_FOUND, ENOENT},
    {
        ERROR_PRIVILEGE_NOT_HELD,
        EACCES,
    },
    {
        ERROR_CANT_RESOLVE_FILENAME,
        ELOOP,
    },
    {WSAEINTR, EINTR},
    {WSAEBADF, EBADF},
    {WSAEACCES, EACCES},
    {WSAEFAULT, EFAULT},
    {WSAEINVAL, EINVAL},
    {WSAEMFILE, EMFILE},
    {WSAEWOULDBLOCK, EWOULDBLOCK},
    {WSAEINPROGRESS, EINPROGRESS},
    {WSAEALREADY, EALREADY},
    {WSAENOTSOCK, ENOTSOCK},
    {WSAEDESTADDRREQ, EDESTADDRREQ},
    {WSAEMSGSIZE, EMSGSIZE},
    {WSAEPROTOTYPE, EPROTOTYPE},
    {WSAENOPROTOOPT, ENOPROTOOPT},
    {WSAEPROTONOSUPPORT, EPROTONOSUPPORT},
    {WSAESOCKTNOSUPPORT, EINVAL /*ESOCKTNOSUPPORT*/},
    {WSAEOPNOTSUPP, EOPNOTSUPP},
    {WSAEPFNOSUPPORT, EINVAL /*EPFNOSUPPORT*/},
    {WSAEAFNOSUPPORT, EAFNOSUPPORT},
    {WSAEADDRINUSE, EADDRINUSE},
    {WSAEADDRNOTAVAIL, EADDRNOTAVAIL},
    {WSAENETDOWN, ENETDOWN},
    {WSAENETUNREACH, ENETUNREACH},
    {WSAENETRESET, ENETRESET},
    {WSAECONNABORTED, ECONNABORTED},
    {WSAECONNRESET, ECONNRESET},
    {WSAENOBUFS, ENOBUFS},
    {WSAEISCONN, EISCONN},
    {WSAENOTCONN, ENOTCONN},
    {WSAESHUTDOWN, EINVAL /*ESHUTDOWN*/},
    {WSAETOOMANYREFS, EINVAL /*ETOOMANYREFS*/},
    {WSAETIMEDOUT, ETIMEDOUT},
    {WSAECONNREFUSED, ECONNREFUSED},
    {WSAELOOP, ELOOP},
    {WSAENAMETOOLONG, ENAMETOOLONG},
    {WSAEHOSTDOWN, EINVAL /*EHOSTDOWN*/},
    {WSAEHOSTUNREACH, EHOSTUNREACH},
    {WSAEPROCLIM, EINVAL /*EPROCLIM*/},
    {WSAENOTEMPTY, ENOTEMPTY},
    {WSAEUSERS, EINVAL /*EUSERS*/},
    {WSAEDQUOT, EINVAL /*EDQUOT*/},
    {WSAESTALE, EINVAL /*ESTALE*/},
    {WSAEREMOTE, EINVAL /*EREMOTE*/},
};

static int have_precisetime = -1;

static void get_systemtime(FILETIME* ft)
{
  typedef void(WINAPI * get_time_func)(FILETIME * ft);
  static get_time_func func = (get_time_func)-1;

  if (func == (get_time_func)-1) {
    /* GetSystemTimePreciseAsFileTime is available since Windows 8 and Windows
     * Server 2012. */
    func = (get_time_func)get_proc_address("kernel32", "GetSystemTimePreciseAsFileTime", NULL);
    if (func == NULL) {
      func = GetSystemTimeAsFileTime;
      have_precisetime = 0;
    }
    else
      have_precisetime = 1;
  }
  if (!ft)
    return;
  func(ft);
}

//
// UNIX compatible directory access functions for NT
//

typedef DWORD(WINAPI* get_final_path_func)(HANDLE, WCHAR*, DWORD, DWORD);
static get_final_path_func get_final_path;

static DWORD WINAPI get_final_path_fail(HANDLE f, WCHAR* buf, DWORD len, DWORD flag)
{
  return 0;
}

static DWORD WINAPI get_final_path_unknown(HANDLE f, WCHAR* buf, DWORD len, DWORD flag)
{
  /* Since Windows Vista and Windows Server 2008 */
  get_final_path_func func = (get_final_path_func)get_proc_address("kernel32", "GetFinalPathNameByHandleW", NULL);
  if (!func)
    func = get_final_path_fail;
  get_final_path = func;
  return func(f, buf, len, flag);
}

static get_final_path_func get_final_path = get_final_path_unknown;

/* License: Ruby's */
static int rb_w32_map_errno(DWORD winerr)
{
  int i;

  if (winerr == 0) {
    return 0;
  }

  for (i = 0; i < (int)(sizeof(errmap) / sizeof(*errmap)); i++) {
    if (errmap[i].winerr == winerr) {
      return errmap[i].err;
    }
  }

  if (winerr >= WSABASEERR) {
    return winerr;
  }
  return EINVAL;
}

/* License: Ruby's */
static inline WCHAR* translate_wchar(WCHAR* p, int from, int to)
{
  for (; *p; p++) {
    if (*p == from)
      *p = to;
  }
  return p;
}

/* License: Ruby's */
static inline char* translate_char(char* p, int from, int to, UINT cp)
{
  while (*p) {
    if ((unsigned char)*p == from)
      *p = to;
    p = CharNextExA(cp, p, 0);
  }
  return p;
}

/* License: Ruby's */
static void* getcwd_alloc(int size, void* dummy)
{
  return malloc(size);
}

/* License: Ruby's */
/* TODO: better name */
static HANDLE open_special(const WCHAR* path, DWORD access, DWORD flags)
{
  const DWORD share_mode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
  return CreateFileW(path, access, share_mode, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | flags, NULL);
}

/* License: Ruby's */
static FARPROC get_proc_address(const char* module, const char* func, HANDLE* mh)
{
  HANDLE h;
  FARPROC ptr;

  if (mh)
    h = LoadLibrary(module);
  else
    h = GetModuleHandle(module);
  if (!h)
    return NULL;

  ptr = GetProcAddress(h, func);
  if (mh) {
    if (ptr)
      *mh = h;
    else
      FreeLibrary(h);
  }
  return ptr;
}

/* License: Ruby's */
static unsigned fileattr_to_unixmode(DWORD attr, const WCHAR* path, unsigned mode)
{
  if (attr & FILE_ATTRIBUTE_READONLY) {
    mode |= S_IREAD;
  }
  else {
    mode |= S_IREAD | S_IWRITE | S_IWUSR;
  }

  if (mode & S_IFMT) {
    /* format is already set */
  }
  /*    else if (attr & FILE_ATTRIBUTE_REPARSE_POINT) {
          if (rb_w32_reparse_symlink_p(path))
              mode |= S_IFLNK | S_IEXEC;
          else
              mode |= S_IFDIR | S_IEXEC;
      }
  */
  else if (attr & FILE_ATTRIBUTE_DIRECTORY) {
    mode |= S_IFDIR | S_IEXEC;
  }
  else {
    mode |= S_IFREG;
  }

  if (path && (mode & S_IFREG)) {
    const WCHAR* end = path + lstrlenW(path);
    while (path < end) {
      end = CharPrevW(path, end);
      if (*end == L'.') {
        if ((_wcsicmp(end, L".bat") == 0) || (_wcsicmp(end, L".cmd") == 0) || (_wcsicmp(end, L".com") == 0) ||
            (_wcsicmp(end, L".exe") == 0)) {
          mode |= S_IEXEC;
        }
        break;
      }
      if (!iswalnum(*end))
        break;
    }
  }

  mode |= (mode & 0500) >> 3;
  mode |= (mode & 0500) >> 6;

  return mode;
}

/* License: Ruby's */
static time_t filetime_to_unixtime(const FILETIME* ft)
{
  long subsec;
  time_t t = filetime_split(ft, &subsec);

  if (t < 0)
    return 0;
  return t;
}

/* License: Ruby's */
/* split FILETIME value into UNIX time and sub-seconds in NT ticks */
static time_t filetime_split(const FILETIME* ft, long* subsec)
{
  ULARGE_INTEGER tmp;
  unsigned LONG_LONG lt;
  const unsigned LONG_LONG subsec_unit = (unsigned LONG_LONG)10 * 1000 * 1000;

  tmp.LowPart = ft->dwLowDateTime;
  tmp.HighPart = ft->dwHighDateTime;
  lt = tmp.QuadPart;

  /* lt is now 100-nanosec intervals since 1601/01/01 00:00:00 UTC,
     convert it into UNIX time (since 1970/01/01 00:00:00 UTC).
     the first leap second is at 1972/06/30, so we doesn't need to think
     about it. */
  lt -= (LONG_LONG)((1970 - 1601) * 365.2425) * 24 * 60 * 60 * subsec_unit;

  *subsec = (long)(lt % subsec_unit);
  return (time_t)(lt / subsec_unit);
}

/* License: Ruby's */
int __cdecl gettimeofday(struct timeval* tv, struct timezone* tz)
{
  FILETIME ft;
  long subsec;

  get_systemtime(&ft);
  tv->tv_sec = filetime_split(&ft, &subsec);
  tv->tv_usec = subsec / 10;

  return 0;
}

/* License: Ruby's */
static long filetime_to_nsec(const FILETIME* ft)
{
  if (have_precisetime <= 0)
    return 0;
  else {
    ULARGE_INTEGER tmp;
    tmp.LowPart = ft->dwLowDateTime;
    tmp.HighPart = ft->dwHighDateTime;
    return (long)(tmp.QuadPart % 10000000) * 100;
  }
}

/* License: Ruby's */
static int check_valid_dir(const WCHAR* path)
{
  WIN32_FIND_DATAW fd;
  HANDLE fh;
  WCHAR full[PATH_MAX];
  WCHAR* dmy;
  WCHAR *p, *q;

  /* GetFileAttributes() determines "..." as directory. */
  /* We recheck it by FindFirstFile(). */
  if (!(p = wcsstr(path, L"...")))
    return 0;
  q = p + wcsspn(p, L".");
  if ((p == path || wcschr(L":/\\", *(p - 1))) && (!*q || wcschr(L":/\\", *q))) {
    errno = ENOENT;
    return -1;
  }

  /* if the specified path is the root of a drive and the drive is empty, */
  /* FindFirstFile() returns INVALID_HANDLE_VALUE. */
  if (!GetFullPathNameW(path, sizeof(full) / sizeof(WCHAR), full, &dmy)) {
    errno = map_errno(GetLastError());
    return -1;
  }
  if (full[1] == L':' && !full[3] && GetDriveTypeW(full) != DRIVE_NO_ROOT_DIR)
    return 0;

  fh = open_dir_handle(path, &fd);
  if (fh == INVALID_HANDLE_VALUE)
    return -1;
  FindClose(fh);
  return 0;
}

/* License: Artistic or GPL */
static HANDLE open_dir_handle(const WCHAR* filename, WIN32_FIND_DATAW* fd)
{
  HANDLE fh;
  WCHAR fullname[FINAL_PATH_MAX + sizeof("\\*")];
  WCHAR* p;
  int len = 0;

  //
  // Create the search pattern
  //

  fh = open_special(filename, 0, 0);
  if (fh != INVALID_HANDLE_VALUE) {
    len = get_final_path(fh, fullname, FINAL_PATH_MAX, 0);
    CloseHandle(fh);
    if (len >= FINAL_PATH_MAX) {
      errno = ENAMETOOLONG;
      return INVALID_HANDLE_VALUE;
    }
  }
  if (!len) {
    len = lstrlenW(filename);
    if (len >= PATH_MAX) {
      errno = ENAMETOOLONG;
      return INVALID_HANDLE_VALUE;
    }
    memcpy(fullname, filename, sizeof(WCHAR) * len);
  }
  p = &fullname[len - 1];
  if (!(isdirsep(*p) || *p == L':'))
    *++p = L'\\';
  *++p = L'*';
  *++p = L'\0';

  //
  // do the FindFirstFile call
  //
  fh = FindFirstFileW(fullname, fd);
  if (fh == INVALID_HANDLE_VALUE) {
    errno = map_errno(GetLastError());
  }
  return fh;
}

/* License: Ruby's */
WCHAR* rb_w32_mbstr_to_wstr(UINT cp, const char* str, int clen, long* plen)
{
  /* This is used by MJIT worker. Do not trigger GC or call Ruby method here. */
  WCHAR* ptr;
  int len = MultiByteToWideChar(cp, 0, str, clen, NULL, 0);
  if (!(ptr = malloc(sizeof(WCHAR) * len)))
    return 0;
  MultiByteToWideChar(cp, 0, str, clen, ptr, len);
  if (plen) {
    /* exclude NUL only if NUL-terminated string */
    if (clen == -1)
      --len;
    *plen = len;
  }
  return ptr;
}

/* License: Ruby's */
static int isUNCRoot(const WCHAR* path)
{
  if (path[0] == L'\\' && path[1] == L'\\') {
    const WCHAR* p = path + 2;
    if (p[0] == L'?' && p[1] == L'\\') {
      p += 2;
    }
    for (; *p; p++) {
      if (*p == L'\\')
        break;
    }
    if (p[0] && p[1]) {
      for (p++; *p; p++) {
        if (*p == L'\\')
          break;
      }
      if (!p[0] || !p[1] || (p[1] == L'.' && !p[2]))
        return 1;
    }
  }
  return 0;
}

/* License: Ruby's */
static char* w32_getcwd(char* buffer, int size, UINT cp, void* alloc(int, void*), void* arg)
{
  WCHAR* p;
  int wlen, len;

  len = GetCurrentDirectoryW(0, NULL);
  if (!len) {
    errno = map_errno(GetLastError());
    return NULL;
  }

  if (buffer && size < len) {
    errno = ERANGE;
    return NULL;
  }

  p = ALLOCA_N(WCHAR, len);
  if (!GetCurrentDirectoryW(len, p)) {
    errno = map_errno(GetLastError());
    return NULL;
  }

  wlen = translate_wchar(p, L'\\', L'/') - p + 1;
  len = WideCharToMultiByte(cp, 0, p, wlen, NULL, 0, NULL, NULL);
  if (buffer) {
    if (size < len) {
      errno = ERANGE;
      return NULL;
    }
  }
  else {
    buffer = (*alloc)(len, arg);
    if (!buffer) {
      errno = ENOMEM;
      return NULL;
    }
  }
  WideCharToMultiByte(cp, 0, p, wlen, buffer, len, NULL, NULL);

  return buffer;
}

/* License: Ruby's */
char* rb_w32_ugetcwd(char* buffer, int size)
{
  return w32_getcwd(buffer, size, CP_UTF8, getcwd_alloc, NULL);
}

/* License: Ruby's */
int rb_w32_uchdir(const char* path)
{
  WCHAR* wpath;
  int ret;

  if (!(wpath = utf8_to_wstr(path, NULL)))
    return -1;
  ret = _wchdir(wpath);
  free(wpath);
  return ret;
}

/* License: Ruby's */
int rb_w32_ustati128(const char* path, struct stati128* st)
{
  return w32_stati128(path, st, CP_UTF8, FALSE);
}

/* License: Ruby's */
int rb_w32_stati128(const char* path, struct stati128* st)
{
  return w32_stati128(path, st, filecp(), FALSE);
}

/* License: Ruby's */
static int w32_stati128(const char* path, struct stati128* st, UINT cp, BOOL lstat)
{
  WCHAR* wpath;
  int ret;

  if (!(wpath = mbstr_to_wstr(cp, path, -1, NULL)))
    return -1;
  ret = wstati128(wpath, st, lstat);
  free(wpath);
  return ret;
}

/* License: Ruby's */
int rb_w32_ulstati128(const char* path, struct stati128* st)
{
  return w32_stati128(path, st, CP_UTF8, TRUE);
}

/* License: Ruby's */
int rb_w32_lstati128(const char* path, struct stati128* st)
{
  return w32_stati128(path, st, filecp(), TRUE);
}

/* License: Ruby's */
static WCHAR* name_for_stat(WCHAR* buf1, const WCHAR* path)
{
  const WCHAR* p;
  WCHAR *s, *end;
  int len;

  for (p = path, s = buf1; *p; p++, s++) {
    if (*p == L'/')
      *s = L'\\';
    else
      *s = *p;
  }
  *s = '\0';
  len = s - buf1;
  if (!len || L'\"' == *(--s)) {
    errno = ENOENT;
    return NULL;
  }
  end = buf1 + len - 1;

  if (isUNCRoot(buf1)) {
    if (*end == L'.')
      *end = L'\0';
    else if (*end != L'\\')
      lstrcatW(buf1, L"\\");
  }
  else if (*end == L'\\' || (buf1 + 1 == end && *end == L':'))
    lstrcatW(buf1, L".");

  return buf1;
}

/* License: Ruby's */
static int stat_by_find(const WCHAR* path, struct stati128* st)
{
  HANDLE h;
  WIN32_FIND_DATAW wfd;
  /* GetFileAttributesEx failed; check why. */
  int e = GetLastError();

  if ((e == ERROR_FILE_NOT_FOUND) || (e == ERROR_INVALID_NAME) ||
      (e == ERROR_PATH_NOT_FOUND || (e == ERROR_BAD_NETPATH))) {
    errno = map_errno(e);
    return -1;
  }

  /* Fall back to FindFirstFile for ERROR_SHARING_VIOLATION */
  h = FindFirstFileW(path, &wfd);
  if (h == INVALID_HANDLE_VALUE) {
    errno = map_errno(GetLastError());
    return -1;
  }
  FindClose(h);
  st->st_mode = fileattr_to_unixmode(wfd.dwFileAttributes, path, 0);
  st->st_atime = filetime_to_unixtime(&wfd.ftLastAccessTime);
  st->st_atimensec = filetime_to_nsec(&wfd.ftLastAccessTime);
  st->st_mtime = filetime_to_unixtime(&wfd.ftLastWriteTime);
  st->st_mtimensec = filetime_to_nsec(&wfd.ftLastWriteTime);
  st->st_ctime = filetime_to_unixtime(&wfd.ftCreationTime);
  st->st_ctimensec = filetime_to_nsec(&wfd.ftCreationTime);
  st->st_size = ((__int64)wfd.nFileSizeHigh << 32) | wfd.nFileSizeLow;
  st->st_nlink = 1;
  return 0;
}

/* License: Ruby's */
static int path_drive(const WCHAR* path)
{
  return (iswalpha(path[0]) && path[1] == L':') ? towupper(path[0]) - L'A' : _getdrive() - 1;
}

/* License: Ruby's */
static int winnt_stat(const WCHAR* path, struct stati128* st, BOOL lstat)
{
  DWORD flags = lstat ? FILE_FLAG_OPEN_REPARSE_POINT : 0;
  HANDLE f;
  WCHAR finalname[PATH_MAX];

  memset(st, 0, sizeof(*st));
  f = open_special(path, 0, flags);
  if (f != INVALID_HANDLE_VALUE) {
    DWORD attr = stati128_handle(f, st);
    const DWORD len = get_final_path(f, finalname, numberof(finalname), 0);
    unsigned mode = 0;
    switch (GetFileType(f)) {
      case FILE_TYPE_CHAR:
        mode = S_IFCHR;
        break;
      case FILE_TYPE_PIPE:
        mode = S_IFIFO;
        break;
    }
    CloseHandle(f);
    /*	if (attr & FILE_ATTRIBUTE_REPARSE_POINT) {
                if (rb_w32_reparse_symlink_p(path))
                    st->st_size = 0;
                else
                    attr &= ~FILE_ATTRIBUTE_REPARSE_POINT;
            }
    */
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
      if (check_valid_dir(path))
        return -1;
    }
    st->st_mode = fileattr_to_unixmode(attr, path, mode);
    if (len) {
      finalname[min(len, numberof(finalname) - 1)] = L'\0';
      path = finalname;
      if (wcsncmp(path, namespace_prefix, numberof(namespace_prefix)) == 0)
        path += numberof(namespace_prefix);
    }
  }
  else {
    if (stat_by_find(path, st))
      return -1;
  }

  st->st_dev = st->st_rdev = path_drive(path);

  return 0;
}

/* License: Ruby's */
static int wstati128(const WCHAR* path, struct stati128* st, BOOL lstat)
{
  WCHAR* buf1;
  int ret, size;

  if (!path || !st) {
    errno = EFAULT;
    return -1;
  }
  size = lstrlenW(path) + 2;
  buf1 = ALLOCA_N(WCHAR, size * 10);
  if (!(path = name_for_stat(buf1, path)))
    return -1;
  ret = winnt_stat(path, st, lstat);
  return ret;
}

/* License: Ruby's */
static int w32_access(const char* path, int mode, UINT cp)
{
  struct stati128 stat;
  if (w32_stati128(path, &stat, cp, FALSE) != 0)
    return -1;
  mode <<= 6;
  if ((stat.st_mode & mode) != mode) {
    errno = EACCES;
    return -1;
  }
  return 0;
}

/* License: Ruby's */
int rb_w32_access(const char* path, int mode)
{
  return w32_access(path, mode, filecp());
}

/* License: Ruby's */
int rb_w32_uaccess(const char* path, int mode)
{
  return w32_access(path, mode, CP_UTF8);
}

/* License: Ruby's */
static DWORD stati128_handle(HANDLE h, struct stati128* st)
{
  BY_HANDLE_FILE_INFORMATION info;
  DWORD attr = (DWORD)-1;

  if (GetFileInformationByHandle(h, &info)) {
    FILE_ID_INFO fii;
    st->st_size = ((__int64)info.nFileSizeHigh << 32) | info.nFileSizeLow;
    st->st_atime = filetime_to_unixtime(&info.ftLastAccessTime);
    st->st_atimensec = filetime_to_nsec(&info.ftLastAccessTime);
    st->st_mtime = filetime_to_unixtime(&info.ftLastWriteTime);
    st->st_mtimensec = filetime_to_nsec(&info.ftLastWriteTime);
    st->st_ctime = filetime_to_unixtime(&info.ftCreationTime);
    st->st_ctimensec = filetime_to_nsec(&info.ftCreationTime);
    st->st_nlink = info.nNumberOfLinks;
    attr = info.dwFileAttributes;
    if (!get_ino(h, &fii)) {
      st->st_ino = *((unsigned __int64*)&fii.FileId);
      st->st_inohigh = *((__int64*)&fii.FileId + 1);
    }
    else {
      st->st_ino = ((__int64)info.nFileIndexHigh << 32) | info.nFileIndexLow;
      st->st_inohigh = 0;
    }
  }
  return attr;
}

static DWORD get_ino(HANDLE h, FILE_ID_INFO* id)
{
  typedef BOOL(WINAPI * gfibhe_t)(HANDLE, int, void*, DWORD);
  static gfibhe_t pGetFileInformationByHandleEx = (gfibhe_t)-1;

  if (pGetFileInformationByHandleEx == (gfibhe_t)-1)
    /* Since Windows Vista and Windows Server 2008 */
    pGetFileInformationByHandleEx = (gfibhe_t)get_proc_address("kernel32", "GetFileInformationByHandleEx", NULL);

  if (pGetFileInformationByHandleEx) {
    if (pGetFileInformationByHandleEx(h, FileIdInfo, id, sizeof(*id)))
      return 0;
    else
      return GetLastError();
  }
  return ERROR_INVALID_PARAMETER;
}

/* License: Ruby's */
UINT filecp(void)
{
  /* UINT cp = AreFileApisANSI() ? CP_ACP : CP_OEMCP;
  return cp; */
  return CP_ACP;
}

/* License: Ruby's */
int rb_w32_uopen(const char* file, int oflag, ...)
{
  WCHAR* wfile;
  int ret;
  int pmode;

  va_list arg;
  va_start(arg, oflag);
  pmode = va_arg(arg, int);
  va_end(arg);

  if (!(wfile = utf8_to_wstr(file, NULL)))
    return -1;
  ret = w32_wopen(wfile, oflag, pmode);
  free(wfile);
  return ret;
}

/* License: Ruby's */
int rb_w32_wopen(const WCHAR* file, int oflag, ...)
{
  int pmode = 0;

  if (oflag & O_CREAT) {
    va_list arg;
    va_start(arg, oflag);
    pmode = va_arg(arg, int);
    va_end(arg);
  }

  return w32_wopen(file, oflag, pmode);
}

static int w32_wopen(const WCHAR* file, int oflag, int pmode)
{
  char flags = 0;
  int fd;
  DWORD access;
  DWORD create;
  DWORD attr = FILE_ATTRIBUTE_NORMAL;
  SECURITY_ATTRIBUTES sec;
  HANDLE h;
  int share_delete;

  share_delete = oflag & O_SHARE_DELETE ? FILE_SHARE_DELETE : 0;
  oflag &= ~O_SHARE_DELETE;
  if ((oflag & O_TEXT) || !(oflag & O_BINARY)) {
    fd = _wopen(file, oflag, pmode);
    if (fd == -1) {
      switch (errno) {
        case EACCES:
          check_if_wdir(file);
          break;
        case EINVAL:
          errno = map_errno(GetLastError());
          break;
      }
    }
    return fd;
  }

  sec.nLength = sizeof(sec);
  sec.lpSecurityDescriptor = NULL;
  if (oflag & O_NOINHERIT) {
    sec.bInheritHandle = FALSE;
    flags |= FNOINHERIT;
  }
  else {
    sec.bInheritHandle = TRUE;
  }
  oflag &= ~O_NOINHERIT;

  /* always open with binary mode */
  oflag &= ~(O_BINARY | O_TEXT);

  switch (oflag & (O_RDWR | O_RDONLY | O_WRONLY)) {
    case O_RDWR:
      access = GENERIC_READ | GENERIC_WRITE;
      break;
    case O_RDONLY:
      access = GENERIC_READ;
      break;
    case O_WRONLY:
      access = GENERIC_WRITE;
      break;
    default:
      errno = EINVAL;
      return -1;
  }
  oflag &= ~(O_RDWR | O_RDONLY | O_WRONLY);

  switch (oflag & (O_CREAT | O_EXCL | O_TRUNC)) {
    case O_CREAT:
      create = OPEN_ALWAYS;
      break;
    case 0:
    case O_EXCL:
      create = OPEN_EXISTING;
      break;
    case O_CREAT | O_EXCL:
    case O_CREAT | O_EXCL | O_TRUNC:
      create = CREATE_NEW;
      break;
    case O_TRUNC:
    case O_TRUNC | O_EXCL:
      create = TRUNCATE_EXISTING;
      break;
    case O_CREAT | O_TRUNC:
      create = CREATE_ALWAYS;
      break;
    default:
      errno = EINVAL;
      return -1;
  }
  if (oflag & O_CREAT) {
    /* TODO: we need to check umask here, but it's not exported... */
    if (!(pmode & S_IWRITE))
      attr = FILE_ATTRIBUTE_READONLY;
  }
  oflag &= ~(O_CREAT | O_EXCL | O_TRUNC);

  if (oflag & O_TEMPORARY) {
    attr |= FILE_FLAG_DELETE_ON_CLOSE;
    access |= DELETE;
  }
  oflag &= ~O_TEMPORARY;

  if (oflag & _O_SHORT_LIVED)
    attr |= FILE_ATTRIBUTE_TEMPORARY;
  oflag &= ~_O_SHORT_LIVED;

  switch (oflag & (O_SEQUENTIAL | O_RANDOM)) {
    case 0:
      break;
    case O_SEQUENTIAL:
      attr |= FILE_FLAG_SEQUENTIAL_SCAN;
      break;
    case O_RANDOM:
      attr |= FILE_FLAG_RANDOM_ACCESS;
      break;
    default:
      errno = EINVAL;
      return -1;
  }
  oflag &= ~(O_SEQUENTIAL | O_RANDOM);

  if (oflag & ~O_APPEND) {
    errno = EINVAL;
    return -1;
  }

  /* allocate a C Runtime file handle */
  RUBY_CRITICAL
  {
    h = CreateFile("NUL", 0, 0, NULL, OPEN_ALWAYS, 0, NULL);
    fd = _open_osfhandle((intptr_t)h, 0);
    CloseHandle(h);
  }
  if (fd == -1) {
    errno = EMFILE;
    return -1;
  }
  RUBY_CRITICAL
  {
    // rb_acrt_lowio_lock_fh(fd);
    _set_osfhnd(fd, (intptr_t)INVALID_HANDLE_VALUE);
    _set_osflags(fd, 0);

    h = CreateFileW(file, access, FILE_SHARE_READ | FILE_SHARE_WRITE | share_delete, &sec, create, attr, NULL);
    if (h == INVALID_HANDLE_VALUE) {
      DWORD e = GetLastError();
      if (e != ERROR_ACCESS_DENIED || !check_if_wdir(file))
        errno = map_errno(e);
      // rb_acrt_lowio_unlock_fh(fd);
      fd = -1;
      goto quit;
    }

    switch (GetFileType(h)) {
      case FILE_TYPE_CHAR:
        flags |= FDEV;
        break;
      case FILE_TYPE_PIPE:
        flags |= FPIPE;
        break;
      case FILE_TYPE_UNKNOWN:
        errno = map_errno(GetLastError());
        CloseHandle(h);
        // rb_acrt_lowio_unlock_fh(fd);
        fd = -1;
        goto quit;
    }
    if (!(flags & (FDEV | FPIPE)) && (oflag & O_APPEND))
      flags |= FAPPEND;

    _set_osfhnd(fd, (intptr_t)h);
    _set_osflags(fd, flags | FOPEN);

    // rb_acrt_lowio_unlock_fh(fd);
  quit:;
  }

  return fd;
}

/* License: Ruby's */
static int check_if_wdir(const WCHAR* wfile)
{
  DWORD attr = GetFileAttributesW(wfile);
  if (attr == (DWORD)-1L || !(attr & FILE_ATTRIBUTE_DIRECTORY) || check_valid_dir(wfile)) {
    return FALSE;
  }
  errno = EISDIR;
  return TRUE;
}

int rb_w32_close(int fd)
{
  return _close(fd);
}

#undef read
ssize_t rb_w32_read(int fd, void* buf, size_t size)
{
  return _read(fd, buf, size);
}

ssize_t rb_w32_pread(int fd, void* buf, size_t size, size_t /* offset */)
{
  return _read(fd, buf, size);
}

off_t rb_w32_lseek(int fd, off_t ofs, int whence)
{
  return _lseeki64(fd, ofs, whence);
}

#define COPY_STAT(src, dest, size_cast)      \
  do {                                       \
    (dest).st_dev = (src).st_dev;            \
    (dest).st_ino = (src).st_ino;            \
    (dest).st_mode = (src).st_mode;          \
    (dest).st_nlink = (src).st_nlink;        \
    (dest).st_uid = (src).st_uid;            \
    (dest).st_gid = (src).st_gid;            \
    (dest).st_rdev = (src).st_rdev;          \
    (dest).st_size = size_cast(src).st_size; \
    (dest).st_atime = (src).st_atime;        \
    (dest).st_mtime = (src).st_mtime;        \
    (dest).st_ctime = (src).st_ctime;        \
  } while (0)

/* License: Ruby's */
#undef fstat
int rb_w32_fstati128(int fd, struct stati128* st)
{
  struct stat tmp;
  int ret = fstat(fd, &tmp);

  if (ret)
    return ret;
  COPY_STAT(tmp, *st, +);
  stati128_handle((HANDLE)_get_osfhandle(fd), st);
  return ret;
}

//
// The idea here is to read all the directory names into a string table
// (separated by nulls) and when one of the other dir functions is called
// return the pointer to the current file name.
//

/* License: Ruby's */
#define GetBit(bits, i) ((bits)[(i) / CHAR_BIT] & (1 << (i) % CHAR_BIT))
#define SetBit(bits, i) ((bits)[(i) / CHAR_BIT] |= (1 << (i) % CHAR_BIT))

#define BitOfIsDir(n) ((n)*2)
#define BitOfIsRep(n) ((n)*2 + 1)
#define DIRENT_PER_CHAR (CHAR_BIT / 2)

/* License: Artistic or GPL */
static DIR* w32_wopendir(const WCHAR* wpath)
{
  struct stati128 sbuf;
  WIN32_FIND_DATAW fd;
  HANDLE fh;
  DIR* p;
  long pathlen;
  long len;
  long altlen;
  long idx;
  WCHAR* tmpW;
  char* tmp;

  //
  // check to see if we've got a directory
  //
  if (wstati128(wpath, &sbuf, FALSE) < 0) {
    return NULL;
  }
  if (!(sbuf.st_mode & S_IFDIR) && (!isalpha(wpath[0]) || wpath[1] != L':' || wpath[2] != L'\0' ||
                                    ((1 << ((wpath[0] & 0x5f) - 'A')) & GetLogicalDrives()) == 0)) {
    errno = ENOTDIR;
    return NULL;
  }
  fh = open_dir_handle(wpath, &fd);
  if (fh == INVALID_HANDLE_VALUE) {
    return NULL;
  }

  //
  // Get us a DIR structure
  //
  p = calloc(sizeof(DIR), 1);
  if (p == NULL)
    return NULL;

  pathlen = lstrlenW(wpath);
  idx = 0;

  //
  // loop finding all the files that match the wildcard
  // (which should be all of them in this directory!).
  // the variable idx should point one past the null terminator
  // of the previous string found.
  //
  do {
    len = lstrlenW(fd.cFileName) + 1;
    altlen = lstrlenW(fd.cAlternateFileName) + 1;

    //
    // bump the string table size by enough for the
    // new name and it's null terminator
    //
    tmpW = realloc(p->start, (idx + len + altlen) * sizeof(WCHAR));
    if (!tmpW) {
    error:
      rb_w32_closedir(p);
      FindClose(fh);
      errno = ENOMEM;
      return NULL;
    }

    p->start = tmpW;
    memcpy(&p->start[idx], fd.cFileName, len * sizeof(WCHAR));
    memcpy(&p->start[idx + len], fd.cAlternateFileName, altlen * sizeof(WCHAR));

    if (p->nfiles % DIRENT_PER_CHAR == 0) {
      tmp = realloc(p->bits, p->nfiles / DIRENT_PER_CHAR + 1);
      if (!tmp)
        goto error;
      p->bits = tmp;
      p->bits[p->nfiles / DIRENT_PER_CHAR] = 0;
    }
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      SetBit(p->bits, BitOfIsDir(p->nfiles));
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
      WCHAR* tmppath = malloc((pathlen + len + 1) * sizeof(WCHAR));
      memcpy(tmppath, wpath, pathlen * sizeof(WCHAR));
      tmppath[pathlen] = L'\\';
      memcpy(tmppath + pathlen + 1, fd.cFileName, len * sizeof(WCHAR));
      // if (rb_w32_reparse_symlink_p(tmppath))
      // SetBit(p->bits, BitOfIsRep(p->nfiles));
      free(tmppath);
    }

    p->nfiles++;
    idx += len + altlen;
  } while (FindNextFileW(fh, &fd));
  FindClose(fh);
  p->size = idx;
  p->curr = p->start;
  return p;
}

/* License: Ruby's */
DIR* rb_w32_opendir(const char* filename)
{
  DIR* ret;
  WCHAR* wpath = filecp_to_wstr(filename, NULL);
  if (!wpath)
    return NULL;
  ret = w32_wopendir(wpath);
  free(wpath);
  return ret;
}

/* License: Ruby's */
DIR* rb_w32_uopendir(const char* filename)
{
  DIR* ret;
  WCHAR* wpath = utf8_to_wstr(filename, NULL);
  if (!wpath)
    return NULL;
  ret = w32_wopendir(wpath);
  free(wpath);
  return ret;
}

/* License: Artistic or GPL */
void rb_w32_closedir(DIR* dirp)
{
  if (dirp) {
    if (dirp->dirstr.d_name)
      free(dirp->dirstr.d_name);
    if (dirp->dirstr.d_altname)
      free(dirp->dirstr.d_altname);
    if (dirp->start)
      free(dirp->start);
    if (dirp->bits)
      free(dirp->bits);
    free(dirp);
  }
}

//
// Readdir just returns the current string pointer and bumps the
// string pointer to the next entry.
//
/* License: Ruby's */
static BOOL win32_direct_conv(const WCHAR* file, const WCHAR* alt, struct direct* entry, const void* enc)
{
  UINT cp = *((UINT*)enc);
  if (!(entry->d_name = wstr_to_mbstr(cp, file, -1, &entry->d_namlen)))
    return FALSE;
  if (alt && *alt) {
    long altlen = 0;
    entry->d_altname = wstr_to_mbstr(cp, alt, -1, &altlen);
    entry->d_altlen = altlen;
  }
  return TRUE;
}

/* License: Ruby's */
static BOOL ruby_direct_conv(const WCHAR* file, const WCHAR* alt, struct direct* entry, const void* enc)
{
  if (!(entry->d_name = rb_w32_conv_from_wstr(file, &entry->d_namlen, enc)))
    return FALSE;
  if (alt && *alt) {
    long altlen = 0;
    entry->d_altname = rb_w32_conv_from_wstr(alt, &altlen, enc);
    entry->d_altlen = altlen;
  }
  return TRUE;
}

/* License: Artistic or GPL */
static struct direct* readdir_internal(DIR* dirp,
                                       BOOL (*conv)(const WCHAR*, const WCHAR*, struct direct*, const void*),
                                       const void* enc)
{
  static long dummy_ino = 0;

  if (dirp->curr) {
    //
    // first set up the structure to return
    //
    if (dirp->dirstr.d_name)
      free(dirp->dirstr.d_name);
    if (dirp->dirstr.d_altname)
      free(dirp->dirstr.d_altname);
    dirp->dirstr.d_altname = 0;
    dirp->dirstr.d_altlen = 0;
    conv(dirp->curr, dirp->curr + lstrlenW(dirp->curr) + 1, &dirp->dirstr, enc);

    //
    // Fake inode
    //
    dirp->dirstr.d_ino = (ino_t)(InterlockedIncrement(&dummy_ino) - 1);

    //
    // Attributes
    //
    /* ignore FILE_ATTRIBUTE_DIRECTORY as unreliable for reparse points */
    if (GetBit(dirp->bits, BitOfIsRep(dirp->loc)))
      dirp->dirstr.d_type = DT_LNK;
    else if (GetBit(dirp->bits, BitOfIsDir(dirp->loc)))
      dirp->dirstr.d_type = DT_DIR;
    else
      dirp->dirstr.d_type = DT_REG;

    //
    // Now set up for the next call to readdir
    //

    move_to_next_entry(dirp);

    return &(dirp->dirstr);
  }
  else
    return NULL;
}

/* License: Ruby's */
struct direct* rb_w32_readdir(DIR* dirp, void* enc)
{
  int idx = ENCINDEX_ASCII;  // rb_enc_to_index(enc);
  if (idx == ENCINDEX_ASCII) {
    const UINT cp = filecp();
    return readdir_internal(dirp, win32_direct_conv, &cp);
  }
  else if (idx == ENCINDEX_UTF_8) {
    const UINT cp = CP_UTF8;
    return readdir_internal(dirp, win32_direct_conv, &cp);
  }
  else
    return readdir_internal(dirp, ruby_direct_conv, enc);
}

/* License: Ruby's */
struct direct* rb_w32_ureaddir(DIR* dirp)
{
  const UINT cp = CP_UTF8;
  return readdir_internal(dirp, win32_direct_conv, &cp);
}

/* License: Ruby's */
char* rb_w32_wstr_to_mbstr(UINT cp, const WCHAR* wstr, int clen, long* plen)
{
  char* ptr;
  int len = WideCharToMultiByte(cp, 0, wstr, clen, NULL, 0, NULL, NULL);
  if (!(ptr = malloc(len)))
    return 0;
  WideCharToMultiByte(cp, 0, wstr, clen, ptr, len, NULL, NULL);
  if (plen) {
    /* exclude NUL only if NUL-terminated string */
    if (clen == -1)
      --len;
    *plen = len;
  }
  return ptr;
}

/* License: Ruby's */
char* rb_w32_conv_from_wstr(const WCHAR* wstr, long* lenp, const void* enc)
{
  return wstr_to_utf8(wstr, lenp);
}

/* License: Artistic or GPL */
static void move_to_next_entry(DIR* dirp)
{
  if (dirp->curr) {
    dirp->loc++;
    dirp->curr += lstrlenW(dirp->curr) + 1;
    dirp->curr += lstrlenW(dirp->curr) + 1;
    if (dirp->curr >= (dirp->start + dirp->size)) {
      dirp->curr = NULL;
    }
  }
}

//
// Telldir returns the current string pointer position
//

/* License: Artistic or GPL */
long rb_w32_telldir(DIR* dirp)
{
  return dirp->loc;
}

//
// Seekdir moves the string pointer to a previously saved position
// (Saved by telldir).

/* License: Ruby's */
void rb_w32_seekdir(DIR* dirp, long loc)
{
  if (dirp->loc > loc)
    rb_w32_rewinddir(dirp);

  while (dirp->curr && dirp->loc < loc) {
    move_to_next_entry(dirp);
  }
}

//
// Rewinddir resets the string pointer to the start
//

/* License: Artistic or GPL */
void rb_w32_rewinddir(DIR* dirp)
{
  dirp->curr = dirp->start;
  dirp->loc = 0;
}

/* License: Ruby's */
static int wmkdir(const WCHAR* wpath, int mode)
{
  int ret = -1;

  RUBY_CRITICAL do
  {
    if (CreateDirectoryW(wpath, NULL) == FALSE) {
      errno = map_errno(GetLastError());
      break;
    }
    if (_wchmod(wpath, mode) == -1) {
      RemoveDirectoryW(wpath);
      break;
    }
    ret = 0;
  }
  while (0)
    ;
  return ret;
}

/* License: Ruby's */
int rb_w32_umkdir(const char* path, int mode)
{
  WCHAR* wpath;
  int ret;

  if (!(wpath = utf8_to_wstr(path, NULL)))
    return -1;
  ret = wmkdir(wpath, mode);
  free(wpath);
  return ret;
}

/* License: Ruby's */
int rb_w32_mkdir(const char* path, int mode)
{
  WCHAR* wpath;
  int ret;

  if (!(wpath = filecp_to_wstr(path, NULL)))
    return -1;
  ret = wmkdir(wpath, mode);
  free(wpath);
  return ret;
}

/* License: Ruby's */
static int wrmdir(const WCHAR* wpath)
{
  int ret = 0;
  RUBY_CRITICAL
  {
    const DWORD attr = GetFileAttributesW(wpath);
    if (attr != (DWORD)-1 && (attr & FILE_ATTRIBUTE_READONLY)) {
      SetFileAttributesW(wpath, attr & ~FILE_ATTRIBUTE_READONLY);
    }
    if (RemoveDirectoryW(wpath) == FALSE) {
      errno = map_errno(GetLastError());
      ret = -1;
      if (attr != (DWORD)-1 && (attr & FILE_ATTRIBUTE_READONLY)) {
        SetFileAttributesW(wpath, attr);
      }
    }
  }
  return ret;
}

/* License: Ruby's */
int rb_w32_rmdir(const char* path)
{
  WCHAR* wpath;
  int ret;

  if (!(wpath = filecp_to_wstr(path, NULL)))
    return -1;
  ret = wrmdir(wpath);
  free(wpath);
  return ret;
}

/* License: Ruby's */
int rb_w32_urmdir(const char* path)
{
  WCHAR* wpath;
  int ret;

  if (!(wpath = utf8_to_wstr(path, NULL)))
    return -1;
  ret = wrmdir(wpath);
  free(wpath);
  return ret;
}

/* License: Ruby's */
static int wunlink(const WCHAR* path)
{
  int ret = 0;
  int r = FALSE;
  const DWORD SYMLINKD = FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY;
  RUBY_CRITICAL
  {
    const DWORD attr = GetFileAttributesW(path);
    if (attr == (DWORD)-1) {
    }
    else if ((attr & SYMLINKD) == SYMLINKD) {
      r = RemoveDirectoryW(path);
    }
    else {
      if (attr & FILE_ATTRIBUTE_READONLY) {
        SetFileAttributesW(path, attr & ~FILE_ATTRIBUTE_READONLY);
      }
      r = DeleteFileW(path);
    }
    if (!r) {
      errno = map_errno(GetLastError());
      ret = -1;
      if (attr != (DWORD)-1 && (attr & FILE_ATTRIBUTE_READONLY)) {
        SetFileAttributesW(path, attr);
      }
    }
  }
  return ret;
}

/* License: Ruby's */
int rb_w32_uunlink(const char* path)
{
  WCHAR* wpath;
  int ret;

  if (!(wpath = utf8_to_wstr(path, NULL)))
    return -1;
  ret = wunlink(wpath);
  free(wpath);
  return ret;
}

/* License: Ruby's */
int rb_w32_unlink(const char* path)
{
  WCHAR* wpath;
  int ret;

  if (!(wpath = filecp_to_wstr(path, NULL)))
    return -1;
  ret = wunlink(wpath);
  free(wpath);
  return ret;
}

// It does not matter for tebako
int flock(int, int)
{
  return 0;
}

#endif
