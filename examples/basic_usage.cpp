/**
 * Basic DwarFS Usage Example
 *
 * This example demonstrates the fundamental operations of using libdwarfs:
 * 1. Loading and mounting a DwarFS image
 * 2. Reading a file from the mounted filesystem
 * 3. Properly cleaning up resources
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

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

// Include the main libdwarfs-wr API headers
#include <tebako/fs/io.h>

int main(int argc, char* argv[])
{
  // Check for correct usage
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <dwarfs_image> <file_to_read>\n";
    std::cerr << "Example: " << argv[0] << " filesystem.dwarfs /path/to/file.txt\n";
    return 1;
  }

  const char* image_path = argv[1];
  const char* file_to_read = argv[2];

  std::cout << "=== Basic DwarFS Usage Example ===\n\n";

  // Step 1: Load the DwarFS image into memory
  std::cout << "Step 1: Loading DwarFS image: " << image_path << "\n";

  std::ifstream file(image_path, std::ios::binary | std::ios::ate);
  if (!file) {
    std::cerr << "Error: Failed to open DwarFS image: " << image_path << "\n";
    return 1;
  }

  // Get file size and read into buffer
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<char> buffer(size);
  if (!file.read(buffer.data(), size)) {
    std::cerr << "Error: Failed to read DwarFS image\n";
    return 1;
  }

  std::cout << "  Image size: " << size << " bytes\n";

  // Step 2: Mount the DwarFS image
  std::cout << "\nStep 2: Mounting DwarFS image at root\n";

  /**
   * mount_root_memfs() - Mount a DwarFS filesystem as the root memfs
   *
   * Parameters:
   *   data            - Pointer to the DwarFS image data in memory
   *   size            - Size of the DwarFS image in bytes
   *   debuglevel      - Debug logging level (NULL for default)
   *   cachesize       - Cache size (NULL for default: 512MB)
   *   workers         - Number of worker threads (NULL for default: 2)
   *   mlock           - Memory lock mode (NULL for NONE)
   *   decompress_ratio- Decompression ratio (NULL for default: 0.8)
   *   image_offset    - Image offset (NULL or "auto")
   *
   * Returns: 0 on success, -1 on error
   */
  int ret = mount_root_memfs(
      buffer.data(),  // DwarFS image data
      size,           // Image size
      NULL,           // debuglevel (use default)
      NULL,           // cachesize (use default: 512MB)
      NULL,           // workers (use default: 2)
      NULL,           // mlock (use default: NONE)
      NULL,           // decompress_ratio (use default: 0.8)
      "auto"          // image_offset (auto-detect)
  );

  if (ret != 0) {
    std::cerr << "Error: Failed to mount DwarFS image\n";
    return 1;
  }

  std::cout << "  Successfully mounted DwarFS image\n";

  // Step 3: Open and read a file from the mounted filesystem
  std::cout << "\nStep 3: Reading file: " << file_to_read << "\n";

  /**
   * tebako_open() - Open a file from the mounted DwarFS filesystem
   *
   * Parameters:
   *   nargs - Number of arguments (2 for path+flags, 3 for path+flags+mode)
   *   path  - Path to the file (absolute or relative to current directory)
   *   flags - Open flags (O_RDONLY, O_WRONLY, O_RDWR, etc.)
   *   ...   - Optional mode argument for O_CREAT
   *
   * Returns: File descriptor on success, -1 on error (sets errno)
   */
  int fd = tebako_open(2, file_to_read, O_RDONLY);
  if (fd < 0) {
    std::cerr << "Error: Failed to open file: " << file_to_read << "\n";
    std::cerr << "  errno: " << errno << " (" << strerror(errno) << ")\n";
    unmount_root_memfs();
    return 1;
  }

  std::cout << "  File descriptor: " << fd << "\n";

  // Get file information using fstat
  struct stat st;
  ret = tebako_fstat(fd, &st);
  if (ret == 0) {
    std::cout << "  File size: " << st.st_size << " bytes\n";
    std::cout << "  File mode: " << std::oct << st.st_mode << std::dec << "\n";
  }

  // Read the file contents
  char read_buffer[4096];
  ssize_t bytes_read = tebako_read(fd, read_buffer, sizeof(read_buffer) - 1);

  if (bytes_read < 0) {
    std::cerr << "Error: Failed to read file\n";
    std::cerr << "  errno: " << errno << " (" << strerror(errno) << ")\n";
    tebako_close(fd);
    unmount_root_memfs();
    return 1;
  }

  // Null-terminate the buffer for safe printing
  read_buffer[bytes_read] = '\0';

  std::cout << "  Bytes read: " << bytes_read << "\n";
  std::cout << "\nFile contents:\n";
  std::cout << "--- BEGIN ---\n";
  std::cout << read_buffer;
  if (bytes_read > 0 && read_buffer[bytes_read - 1] != '\n') {
    std::cout << "\n";
  }
  std::cout << "--- END ---\n";

  // Step 4: Close the file
  std::cout << "\nStep 4: Closing file\n";

  /**
   * tebako_close() - Close a file descriptor
   *
   * Parameters:
   *   fd - File descriptor to close
   *
   * Returns: 0 on success, -1 on error (sets errno)
   */
  ret = tebako_close(fd);
  if (ret != 0) {
    std::cerr << "Warning: Failed to close file descriptor\n";
  } else {
    std::cout << "  File closed successfully\n";
  }

  // Step 5: Unmount the filesystem
  std::cout << "\nStep 5: Unmounting DwarFS filesystem\n";

  /**
   * unmount_root_memfs() - Unmount the root DwarFS filesystem
   *
   * This should be called to clean up resources when done with the filesystem.
   * After calling this, all file descriptors and directory handles become invalid.
   */
  unmount_root_memfs();

  std::cout << "  Successfully unmounted filesystem\n";
  std::cout << "\n=== Example completed successfully ===\n";

  return 0;
}