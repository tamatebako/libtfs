// ... existing code ...

#include <iostream>
#include <memory>
#include <fcntl.h>
#include <tebako/fs/backends/dwarfs_backend.h>
#include <tebako/fs/file_handle.h>

int main()
{
  using namespace tebako::fs;

  auto backend = std::make_unique<DwarfsBackend>();

  std::cout << "Step 1: Attempting to mount large.dwarfs..." << std::endl;
  if (!backend->mount("tests/fixtures/dwarfs/large.dwarfs", "/mnt/test")) {
    std::cerr << "FAILED: Could not mount archive" << std::endl;
    return 1;
  }
  std::cout << "SUCCESS: Archive mounted" << std::endl;

  std::cout << "Step 2: Checking if file exists..." << std::endl;
  if (!backend->exists("/mnt/test/10mb.bin")) {
    std::cerr << "FAILED: File does not exist" << std::endl;
    return 1;
  }
  std::cout << "SUCCESS: File exists" << std::endl;

  std::cout << "Step 3: Getting file size..." << std::endl;
  int64_t size = backend->file_size("/mnt/test/10mb.bin");
  std::cout << "File size: " << size << " bytes" << std::endl;

  std::cout << "Step 4: Opening file..." << std::endl;
  auto handle = backend->open("/mnt/test/10mb.bin", O_RDONLY);
  if (!handle) {
    std::cerr << "FAILED: Could not open file" << std::endl;
    return 1;
  }
  std::cout << "SUCCESS: File opened" << std::endl;

  std::cout << "Step 5: Reading first 10 bytes..." << std::endl;
  char buffer[10];
  ssize_t bytes_read = handle->read(buffer, sizeof(buffer));
  std::cout << "Read " << bytes_read << " bytes" << std::endl;

  std::cout << "Step 6: Reading all data in 4KB chunks..." << std::endl;
  char large_buffer[4096];
  int64_t total_read = 0;
  int chunk_count = 0;
  while (true) {
    ssize_t n = handle->read(large_buffer, sizeof(large_buffer));
    if (n <= 0)
      break;
    total_read += n;
    chunk_count++;
    if (chunk_count % 100 == 0) {
      std::cout << "Read " << total_read << " bytes so far..." << std::endl;
    }
  }
  std::cout << "SUCCESS: Read total of " << total_read << " bytes in " << chunk_count << " chunks" << std::endl;

  std::cout << "Step 7: Unmounting..." << std::endl;
  backend->unmount();
  std::cout << "SUCCESS: All tests passed!" << std::endl;

  return 0;
}

//... existing code ...