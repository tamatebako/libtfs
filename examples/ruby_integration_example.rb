#!/usr/bin/env ruby
# Ruby FFI integration example for libtfs

require 'ffi'

module TebakoFS
  extend FFI::Library
  ffi_lib 'tfs'

  # Attach functions (abbreviated for example)
  attach_function :tebako_fs_init_from_file, [:string, :string], :int
  attach_function :tebako_fs_unmount, [], :void
  attach_function :tebako_open, [:string, :int], :int
  attach_function :tebako_read, [:int, :pointer, :size_t], :ssize_t
  attach_function :tebako_close, [:int], :int
  attach_function :tebako_get_errno, [], :int
  attach_function :tebako_strerror, [:int], :string
end

# Mount archive
archive_path = ARGV[0] || "tests/fixtures/zip/simple.zip"
mount_point = "/__tebako__"

puts "Mounting #{archive_path} at #{mount_point}..."
result = TebakoFS.tebako_fs_init_from_file(archive_path, mount_point)

if result != 0
  errno = TebakoFS.tebako_get_errno
  error = TebakoFS.tebako_strerror(errno)
  puts "❌ Mount failed: #{error} (errno=#{errno})"
  exit 1
end

puts "✅ Mounted successfully"

# Open and read file
file_path = "#{mount_point}/test.txt"
puts "\nReading #{file_path}..."

O_RDONLY = 0
fd = TebakoFS.tebako_open(file_path, O_RDONLY)

if fd < 0
  puts "❌ Failed to open file"
  TebakoFS.tebako_fs_unmount
  exit 1
end

# Read content
buffer = FFI::MemoryPointer.new(:char, 1024)
bytes_read = TebakoFS.tebako_read(fd, buffer, 1024)

if bytes_read > 0
  content = buffer.read_string(bytes_read)
  puts "✅ Read #{bytes_read} bytes:"
  puts content
else
  puts "❌ Failed to read file"
end

# Cleanup
TebakoFS.tebako_close(fd)
TebakoFS.tebako_fs_unmount
puts "\n✅ Unmounted successfully"