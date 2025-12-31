/**
 * Comprehensive DwarFS API Example
 *
 * This example demonstrates advanced usage of the libdwarfs API:
 * 1. File operations (open, read, lseek, pread, close)
 * 2. Directory operations (opendir, readdir, closedir)
 * 3. Stat operations (stat, fstat, lstat)
 * 4. Path resolution and navigation (chdir, getcwd)
 * 5. Access checks and file attributes
 *
 * Copyright (c) 2021-2025 [Ribose Inc](https://www.ribose.com).
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
 *    documentation and/or other materials provided with the distribution.
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
 */

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

// Include the main libdwarfs-wr API headers
#include <tebako/fs/io.h>

// Helper function to format file mode
std::string format_mode(mode_t mode)
{
  std::string result;
  result += (S_ISDIR(mode)) ? 'd' : '-';
  result += (mode & S_IRUSR) ? 'r' : '-';
  result += (mode & S_IWUSR) ? 'w' : '-';
  result += (mode & S_IXUSR) ? 'x' : '-';
  result += (mode & S_IRGRP) ? 'r' : '-';
  result += (mode & S_IWGRP) ? 'w' : '-';
  result += (mode & S_IXGRP) ? 'x' : '-';
  result += (mode & S_IROTH) ? 'r' : '-';
  result += (mode & S_IWOTH) ? 'w' : '-';
  result += (mode & S_IXOTH) ? 'x' : '-';
  return result;
}

// Helper function to format file size
std::string format_size(off_t size)
{
  if (size < 1024)
    return std::to_string(size) + " B";
  if (size < 1024 * 1024)
    return std::to_string(size / 1024) + " KB";
  return std::to_string(size / (1024 * 1024)) + " MB";
}

void demonstrate_file_operations(const char* filepath)
{
  std::cout << "\n=== File Operations ===\n\n";

  // 1. Open a file
  std::cout << "1. Opening file: " << filepath << "\n";

  /**
   * tebako_open() opens a file from the DwarFS filesystem
   * Supports standard POSIX flags: O_RDONLY, O_WRONLY, O_RDWR
   * Note: DwarFS is read-only, so O_WRONLY and O_RDWR will fail with EROFS
   */
  int fd = tebako_open(2, filepath, O_RDONLY);
  if (fd < 0) {
    std::cerr << "  Error: Failed to open file (errno=" << errno << ": " << strerror(errno) << ")\n";
    return;
  }
  std::cout << "  Success: File descriptor = " << fd << "\n";

  // 2. Read file content
  std::cout << "\n2. Reading file content (first 256 bytes)\n";

  char buffer[256];
  /**
   * tebako_read() reads data from a file descriptor
   * Returns: number of bytes read, 0 on EOF, -1 on error
   */
  ssize_t bytes_read = tebako_read(fd, buffer, sizeof(buffer) - 1);
  if (bytes_read < 0) {
    std::cerr << "  Error: Failed to read file\n";
  } else {
    buffer[bytes_read] = '\0';
    std::cout << "  Bytes read: " << bytes_read << "\n";
    std::cout << "  Content preview: " << std::string(buffer, std::min<size_t>(bytes_read, 50))
              << (bytes_read > 50 ? "..." : "") << "\n";
  }

  // 3. Use lseek to navigate file
  std::cout << "\n3. Using lseek for file navigation\n";

  /**
   * tebako_lseek() repositions the file offset
   * Whence: SEEK_SET (from start), SEEK_CUR (from current), SEEK_END (from end)
   * Returns: new offset, or -1 on error
   */
  off_t new_offset = tebako_lseek(fd, 0, SEEK_END);
  if (new_offset >= 0) {
    std::cout << "  File size (SEEK_END): " << new_offset << " bytes\n";
  }

  new_offset = tebako_lseek(fd, 10, SEEK_SET);
  if (new_offset >= 0) {
    std::cout << "  New offset (SEEK_SET to 10): " << new_offset << "\n";
  }

  // 4. Use pread for random access (if available)
#if defined(TEBAKO_HAS_PREAD) || defined(RB_W32)
  std::cout << "\n4. Using pread for random access\n";

  /**
   * tebako_pread() reads from a specific offset without changing file position
   * Useful for random access without affecting the current offset
   */
  char pread_buffer[32];
  ssize_t pread_bytes = tebako_pread(fd, pread_buffer, sizeof(pread_buffer) - 1, 0);
  if (pread_bytes > 0) {
    pread_buffer[pread_bytes] = '\0';
    std::cout << "  Read " << pread_bytes << " bytes from offset 0\n";
    std::cout << "  Content: " << std::string(pread_buffer, std::min<size_t>(pread_bytes, 30)) << "\n";
  }
#endif

  // 5. Get file statistics
  std::cout << "\n5. Getting file statistics with fstat\n";

  struct stat st;
  /**
   * tebako_fstat() retrieves file information for an open file descriptor
   * Fills in standard stat structure with file metadata
   */
  int ret = tebako_fstat(fd, &st);
  if (ret == 0) {
    std::cout << "  Size: " << st.st_size << " bytes\n";
    std::cout << "  Mode: " << format_mode(st.st_mode) << " (" << std::oct << st.st_mode << std::dec << ")\n";
    std::cout << "  Inode: " << st.st_ino << "\n";
    std::cout << "  Links: " << st.st_nlink << "\n";
  }

  // 6. Close the file
  std::cout << "\n6. Closing file\n";

  /**
   * tebako_close() closes a file descriptor
   * After closing, the file descriptor becomes invalid
   */
  ret = tebako_close(fd);
  if (ret == 0) {
    std::cout << "  Success: File closed\n";
  } else {
    std::cerr << "  Error: Failed to close file\n";
  }
}

void demonstrate_directory_operations(const char* dirpath)
{
  std::cout << "\n=== Directory Operations ===\n\n";

  // 1. Open a directory
  std::cout << "1. Opening directory: " << dirpath << "\n";

  /**
   * tebako_opendir() opens a directory for reading
   * Returns: DIR* pointer on success, NULL on error
   */
  DIR* dirp = tebako_opendir(dirpath);
  if (!dirp) {
    std::cerr << "  Error: Failed to open directory (errno=" << errno << ": " << strerror(errno) << ")\n";
    return;
  }
  std::cout << "  Success: Directory opened\n";

  // 2. Read directory entries
  std::cout << "\n2. Reading directory entries\n";
  std::cout << "  Listing:\n";

  /**
   * tebako_readdir() reads directory entries one at a time
   * Returns: pointer to next entry, or NULL when no more entries
   * The returned pointer is valid until next readdir() or closedir()
   */
  struct dirent* entry;
  int entry_count = 0;
  while ((entry = tebako_readdir(dirp)) != NULL) {
    entry_count++;
    std::cout << "    " << std::setw(3) << entry_count << ". " << entry->d_name;

    // Display entry type if available
#ifdef _DIRENT_HAVE_D_TYPE
    switch (entry->d_type) {
      case DT_DIR:
        std::cout << " (directory)";
        break;
      case DT_REG:
        std::cout << " (file)";
        break;
      case DT_LNK:
        std::cout << " (symlink)";
        break;
      default:
        std::cout << " (other)";
        break;
    }
#endif
    std::cout << "\n";
  }

  std::cout << "  Total entries: " << entry_count << "\n";

  // 3. Use telldir and seekdir for position management
  std::cout << "\n3. Directory position management\n";

  /**
   * tebako_telldir() returns current position in directory stream
   * tebako_seekdir() sets position in directory stream
   * Useful for bookmarking and returning to specific positions
   */
  // Rewind to start by seeking to position 0
  tebako_seekdir(dirp, 0);
  std::cout << "  After seekdir(0), reading first entry again:\n";

  entry = tebako_readdir(dirp);
  if (entry) {
    std::cout << "    " << entry->d_name << "\n";
    long pos = tebako_telldir(dirp);
    std::cout << "  Current position after first read: " << pos << "\n";
  }

  // 4. Close the directory
  std::cout << "\n4. Closing directory\n";

  /**
   * tebako_closedir() closes a directory stream
   * Returns: 0 on success, -1 on error
   */
  int ret = tebako_closedir(dirp);
  if (ret == 0) {
    std::cout << "  Success: Directory closed\n";
  } else {
    std::cerr << "  Error: Failed to close directory\n";
  }
}

void demonstrate_stat_operations(const char* filepath)
{
  std::cout << "\n=== Stat Operations ===\n\n";

  // 1. stat() - follow symlinks
  std::cout << "1. Using stat (follows symlinks)\n";

  struct stat st;
  /**
   * tebako_stat() retrieves file information, following symbolic links
   * If the path is a symlink, it returns info about the target
   */
  int ret = tebako_stat(filepath, &st);
  if (ret == 0) {
    std::cout << "  Path: " << filepath << "\n";
    std::cout << "  Type: " << (S_ISDIR(st.st_mode) ? "directory" : S_ISREG(st.st_mode) ? "file" : "other") << "\n";
    std::cout << "  Size: " << format_size(st.st_size) << "\n";
    std::cout << "  Permissions: " << format_mode(st.st_mode) << "\n";
    std::cout << "  Inode: " << st.st_ino << "\n";
    std::cout << "  Device: " << st.st_dev << "\n";
    std::cout << "  Links: " << st.st_nlink << "\n";

    // Format timestamps
    char time_buf[64];
    struct tm* timeinfo = localtime(&st.st_mtime);
    if (timeinfo) {
      strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", timeinfo);
      std::cout << "  Modified: " << time_buf << "\n";
    }
  } else {
    std::cerr << "  Error: stat failed (errno=" << errno << ": " << strerror(errno) << ")\n";
  }

  // 2. lstat() - don't follow symlinks
#if defined(TEBAKO_HAS_LSTAT) || defined(RB_W32)
  std::cout << "\n2. Using lstat (does not follow symlinks)\n";

  /**
   * tebako_lstat() retrieves file information without following symlinks
   * If the path is a symlink, it returns info about the link itself
   */
  ret = tebako_lstat(filepath, &st);
  if (ret == 0) {
    std::cout << "  Path: " << filepath << "\n";
    if (S_ISLNK(st.st_mode)) {
      std::cout << "  Type: symbolic link\n";

      // Read the link target
      char link_target[PATH_MAX];
      ssize_t len = tebako_readlink(filepath, link_target, sizeof(link_target) - 1);
      if (len > 0) {
        link_target[len] = '\0';
        std::cout << "  Link target: " << link_target << "\n";
      }
    } else {
      std::cout << "  Type: " << (S_ISDIR(st.st_mode) ? "directory" : "regular file") << "\n";
    }
    std::cout << "  Size: " << format_size(st.st_size) << "\n";
  } else {
    std::cerr << "  Error: lstat failed (errno=" << errno << ": " << strerror(errno) << ")\n";
  }
#endif
}

void demonstrate_path_navigation()
{
  std::cout << "\n=== Path Navigation ===\n\n";

  // 1. Get current working directory
  std::cout << "1. Getting current working directory\n";

  char cwd[PATH_MAX];
  /**
   * tebako_getcwd() gets the current working directory
   * Returns: pointer to buffer on success, NULL on error
   */
  char* result = tebako_getcwd(cwd, sizeof(cwd));
  if (result) {
    std::cout << "  Current directory: " << cwd << "\n";
  }

  // 2. Change directory
  std::cout << "\n2. Changing directory\n";

  /**
   * tebako_chdir() changes the current working directory
   * Affects subsequent relative path operations
   * Returns: 0 on success, -1 on error
   */
  int ret = tebako_chdir("/");
  if (ret == 0) {
    std::cout << "  Changed to root directory\n";

    // Verify the change
    result = tebako_getcwd(cwd, sizeof(cwd));
    if (result) {
      std::cout << "  New current directory: " << cwd << "\n";
    }
  } else {
    std::cerr << "  Error: chdir failed (errno=" << errno << ": " << strerror(errno) << ")\n";
  }

  // 3. Access checks
  std::cout << "\n3. Checking file access permissions\n";

  /**
   * tebako_access() checks file accessibility
   * mode: F_OK (existence), R_OK (read), W_OK (write), X_OK (execute)
   * Returns: 0 if access is granted, -1 otherwise
   */
  ret = tebako_access("/", F_OK);
  std::cout << "  Root exists: " << (ret == 0 ? "yes" : "no") << "\n";

  ret = tebako_access("/", R_OK);
  std::cout << "  Root readable: " << (ret == 0 ? "yes" : "no") << "\n";

  ret = tebako_access("/", W_OK);
  std::cout << "  Root writable: " << (ret == 0 ? "no (read-only filesystem)" : "no") << "\n";
}

void demonstrate_advanced_features(const char* dirpath)
{
  std::cout << "\n=== Advanced Features ===\n\n";

#ifdef TEBAKO_HAS_OPENAT
  // 1. openat - open file relative to directory fd
  std::cout << "1. Using openat (open relative to directory fd)\n";

  /**
   * tebako_openat() opens a file relative to a directory file descriptor
   * Useful for avoiding race conditions and working with relative paths
   */
  int dirfd = tebako_open(2, dirpath, O_RDONLY);
  if (dirfd >= 0) {
    std::cout << "  Opened directory fd: " << dirfd << "\n";

    // List directory to find a file
    DIR* dirp = tebako_opendir(dirpath);
    if (dirp) {
      struct dirent* entry = tebako_readdir(dirp);
      if (entry && entry->d_type != DT_DIR) {
        std::cout << "  Opening file '" << entry->d_name << "' relative to directory\n";

        int filefd = tebako_openat(3, dirfd, entry->d_name, O_RDONLY);
        if (filefd >= 0) {
          std::cout << "  Success: File fd = " << filefd << "\n";
          tebako_close(filefd);
        }
      }
      tebako_closedir(dirp);
    }
    tebako_close(dirfd);
  }
#endif

#ifdef TEBAKO_HAS_FSTATAT
  // 2. fstatat - stat relative to directory fd
  std::cout << "\n2. Using fstatat (stat relative to directory fd)\n";

  /**
   * tebako_fstatat() gets file status relative to a directory file descriptor
   * Flags: AT_SYMLINK_NOFOLLOW to not follow symlinks
   */
  int dirfd2 = tebako_open(2, dirpath, O_RDONLY);
  if (dirfd2 >= 0) {
    struct stat st;
    int ret = tebako_fstatat(dirfd2, ".", &st, 0);
    if (ret == 0) {
      std::cout << "  Directory inode: " << st.st_ino << "\n";
      std::cout << "  Directory size: " << st.st_size << "\n";
    }
    tebako_close(dirfd2);
  }
#endif

  // 3. Check if path is within memfs
  std::cout << "\n3. Checking if paths are within DwarFS\n";

  /**
   * within_tebako_memfs() checks if a path is within the mounted DwarFS
   * Returns: 1 if within memfs, 0 otherwise
   */
  int is_memfs = within_tebako_memfs("/");
  std::cout << "  Root is in memfs: " << (is_memfs ? "yes" : "no") << "\n";

  is_memfs = within_tebako_memfs("/tmp");
  std::cout << "  /tmp is in memfs: " << (is_memfs ? "yes" : "no") << "\n";
}

int main(int argc, char* argv[])
{
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <dwarfs_image> [file_path] [dir_path]\n";
    std::cerr << "Example: " << argv[0] << " filesystem.dwarfs /file.txt /directory\n";
    return 1;
  }

  const char* image_path = argv[1];
  const char* file_path = (argc > 2) ? argv[2] : "/file.txt";
  const char* dir_path = (argc > 3) ? argv[3] : "/";

  std::cout << "=== Comprehensive DwarFS API Example ===\n";

  // Load and mount the DwarFS image
  std::ifstream file(image_path, std::ios::binary | std::ios::ate);
  if (!file) {
    std::cerr << "Error: Failed to open DwarFS image: " << image_path << "\n";
    return 1;
  }

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<char> buffer(size);
  if (!file.read(buffer.data(), size)) {
    std::cerr << "Error: Failed to read DwarFS image\n";
    return 1;
  }

  std::cout << "\nMounting DwarFS image (" << format_size(size) << ")...\n";

  int ret = mount_root_memfs(buffer.data(), size, NULL, NULL, NULL, NULL, NULL, "auto");
  if (ret != 0) {
    std::cerr << "Error: Failed to mount DwarFS image\n";
    return 1;
  }

  std::cout << "Successfully mounted!\n";

  // Run demonstrations
  demonstrate_file_operations(file_path);
  demonstrate_directory_operations(dir_path);
  demonstrate_stat_operations(file_path);
  demonstrate_path_navigation();
  demonstrate_advanced_features(dir_path);

  // Cleanup
  std::cout << "\n=== Cleanup ===\n";
  unmount_root_memfs();
  std::cout << "Filesystem unmounted successfully\n";

  std::cout << "\n=== Example completed ===\n";

  return 0;
}