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
 *
 */

#include "tests.h"
#include <tebako/fs/package_descriptor.h>

namespace tebako {

TEST(PackageDescriptorTest, construct_from_buffer)
{
  // Create a buffer with serialized data in big-endian format
  std::vector<char> buffer = {'T', 'A', 'M', 'A', 'T', 'E', 'B', 'A', 'K', 'O',  // signature

                              0x01, 0x00,  // ruby_version_major
                              0x02, 0x00,  // ruby_version_minor
                              0x03, 0x00,  // ruby_version_patch
                              0x04, 0x00,  // tebako_version_major
                              0x05, 0x00,  // tebako_version_minor
                              0x06, 0x00,  // tebako_version_patch
                              // mount_point
                              0x04, 0x00, 'm', 'n', 't', '/',
                              // entry_point
                              0x07, 0x00, 'm', 'a', 'i', 'n', '.', 'r', 'b',
                              // cwd (optional indicator + value)
                              0x01,  // cwd present indicator
                              0x0A, 0x00, '/', 'h', 'o', 'm', 'e', '/', 'u', 's', 'e', 'r'};

  // Convert the buffer to little-endian if necessary
  for (size_t i = 0; i < 6; ++i) {
    uint16_t* value = reinterpret_cast<uint16_t*>(&buffer[i * 2]);
    *value = package_descriptor::to_little_endian(*value);
  }

  // Convert string lengths to little-endian if necessary
  uint16_t* mount_point_length = reinterpret_cast<uint16_t*>(&buffer[12]);
  *mount_point_length = package_descriptor::to_little_endian(*mount_point_length);

  uint16_t* entry_point_length = reinterpret_cast<uint16_t*>(&buffer[18]);
  *entry_point_length = package_descriptor::to_little_endian(*entry_point_length);

  uint16_t* cwd_length = reinterpret_cast<uint16_t*>(&buffer[27]);
  *cwd_length = package_descriptor::to_little_endian(*cwd_length);

  // Construct the package_descriptor from the buffer
  package_descriptor pd(buffer);

  // Verify the values
  EXPECT_EQ(pd.get_ruby_version_major(), 1);
  EXPECT_EQ(pd.get_ruby_version_minor(), 2);
  EXPECT_EQ(pd.get_ruby_version_patch(), 3);
  EXPECT_EQ(pd.get_tebako_version_major(), 4);
  EXPECT_EQ(pd.get_tebako_version_minor(), 5);
  EXPECT_EQ(pd.get_tebako_version_patch(), 6);
  EXPECT_EQ(pd.get_mount_point(), "mnt/");
  EXPECT_EQ(pd.get_entry_point(), "main.rb");
  EXPECT_EQ(pd.get_cwd(), std::optional<std::string>("/home/user"));
}

TEST(PackageDescriptorTest, construct_from_buffer_no_cwd)
{
  // Create a buffer with serialized data in big-endian format
  std::vector<char> buffer = {
      'T', 'A', 'M', 'A', 'T', 'E', 'B', 'A', 'K', 'O',  // signature

      0x01, 0x00,  // ruby_version_major
      0x02, 0x00,  // ruby_version_minor
      0x03, 0x00,  // ruby_version_patch
      0x04, 0x00,  // tebako_version_major
      0x05, 0x00,  // tebako_version_minor
      0x06, 0x00,  // tebako_version_patch
      // mount_point
      0x04, 0x00, 'm', 'n', 't', '/',
      // entry_point
      0x07, 0x00, 'm', 'a', 'i', 'n', '.', 'r', 'b',
      // cwd (optional indicator)
      0x00  // cwd not present indicator
  };

  // Convert the buffer to little-endian if necessary
  for (size_t i = 0; i < 6; ++i) {
    uint16_t* value = reinterpret_cast<uint16_t*>(&buffer[i * 2]);
    *value = package_descriptor::to_little_endian(*value);
  }

  // Convert string lengths to little-endian if necessary
  uint16_t* mount_point_length = reinterpret_cast<uint16_t*>(&buffer[12]);
  *mount_point_length = package_descriptor::to_little_endian(*mount_point_length);

  uint16_t* entry_point_length = reinterpret_cast<uint16_t*>(&buffer[18]);
  *entry_point_length = package_descriptor::to_little_endian(*entry_point_length);

  // Construct the package_descriptor from the buffer
  package_descriptor pd(buffer);

  // Verify the values
  EXPECT_EQ(pd.get_ruby_version_major(), 1);
  EXPECT_EQ(pd.get_ruby_version_minor(), 2);
  EXPECT_EQ(pd.get_ruby_version_patch(), 3);
  EXPECT_EQ(pd.get_tebako_version_major(), 4);
  EXPECT_EQ(pd.get_tebako_version_minor(), 5);
  EXPECT_EQ(pd.get_tebako_version_patch(), 6);
  EXPECT_EQ(pd.get_mount_point(), "mnt/");
  EXPECT_EQ(pd.get_entry_point(), "main.rb");
  EXPECT_EQ(pd.get_cwd(), std::nullopt);
}

TEST(PackageDescriptorTest, serialize)
{
  std::string ruby_version = "3.1.2";
  std::string tebako_version = "2.5.1";
  std::string mount_point = "/app";
  std::string entry_point = "start.rb";
  std::optional<std::string> cwd = "/var/www";

  // Construct the package_descriptor from parameters
  package_descriptor pd(ruby_version, tebako_version, mount_point, entry_point, cwd);

  // Serialize the object
  std::vector<char> buffer = pd.serialize();

  // Verify the serialized data
  size_t expected_size = std::strlen(package_descriptor::signature) + 9 * sizeof(uint16_t) + mount_point.size() +
                         entry_point.size() + sizeof(char) + cwd->size();
  EXPECT_EQ(buffer.size(), expected_size);

  // Verify the signature
  EXPECT_EQ(std::string(buffer.begin(), buffer.begin() + std::strlen(package_descriptor::signature)),
            package_descriptor::signature);

  // Verify the version numbers
  size_t offset = std::strlen(package_descriptor::signature);
  EXPECT_EQ(*reinterpret_cast<uint16_t*>(&buffer[offset]), package_descriptor::to_little_endian(3));
  EXPECT_EQ(*reinterpret_cast<uint16_t*>(&buffer[offset + 2]), package_descriptor::to_little_endian(1));
  EXPECT_EQ(*reinterpret_cast<uint16_t*>(&buffer[offset + 4]), package_descriptor::to_little_endian(2));
  EXPECT_EQ(*reinterpret_cast<uint16_t*>(&buffer[offset + 6]), package_descriptor::to_little_endian(2));
  EXPECT_EQ(*reinterpret_cast<uint16_t*>(&buffer[offset + 8]), package_descriptor::to_little_endian(5));
  EXPECT_EQ(*reinterpret_cast<uint16_t*>(&buffer[offset + 10]), package_descriptor::to_little_endian(1));

  // Verify the mount_point length and value
  offset += 12;
  uint16_t mount_point_length = package_descriptor::to_little_endian(*reinterpret_cast<uint16_t*>(&buffer[offset]));
  EXPECT_EQ(mount_point_length, mount_point.size());
  EXPECT_EQ(std::string(buffer.begin() + offset + 2, buffer.begin() + offset + 2 + mount_point.size()), mount_point);

  // Verify the entry_point length and value
  offset += 2 + mount_point.size();
  uint16_t entry_point_length = package_descriptor::to_little_endian(*reinterpret_cast<uint16_t*>(&buffer[offset]));
  EXPECT_EQ(entry_point_length, entry_point.size());
  EXPECT_EQ(std::string(buffer.begin() + offset + 2, buffer.begin() + offset + 2 + entry_point.size()), entry_point);

  // Verify the cwd indicator, length, and value
  offset += 2 + entry_point.size();
  EXPECT_EQ(buffer[offset], 0x01);
  uint16_t cwd_length = package_descriptor::to_little_endian(*reinterpret_cast<uint16_t*>(&buffer[offset + 1]));
  EXPECT_EQ(cwd_length, cwd->size());
  EXPECT_EQ(std::string(buffer.begin() + offset + 3, buffer.begin() + offset + 3 + cwd->size()), *cwd);
}

TEST(PackageDescriptorTest, serialize_no_cwd)
{
  std::string ruby_version = "3.1.2";
  std::string tebako_version = "2.5.1";
  std::string mount_point = "/app";
  std::string entry_point = "start.rb";
  std::optional<std::string> cwd = std::nullopt;

  // Construct the package_descriptor from parameters
  package_descriptor pd(ruby_version, tebako_version, mount_point, entry_point, cwd);

  // Serialize the object
  std::vector<char> buffer = pd.serialize();

  // Verify the serialized data
  size_t expected_size = std::strlen(package_descriptor::signature) + 8 * sizeof(uint16_t) + mount_point.size() +
                         entry_point.size() + sizeof(char);
  EXPECT_EQ(buffer.size(), expected_size);

  // Verify the signature
  EXPECT_EQ(std::string(buffer.begin(), buffer.begin() + std::strlen(package_descriptor::signature)),
            package_descriptor::signature);

  // Verify the version numbers
  size_t offset = std::strlen(package_descriptor::signature);
  EXPECT_EQ(*reinterpret_cast<uint16_t*>(&buffer[offset]), package_descriptor::to_little_endian(3));
  EXPECT_EQ(*reinterpret_cast<uint16_t*>(&buffer[offset + 2]), package_descriptor::to_little_endian(1));
  EXPECT_EQ(*reinterpret_cast<uint16_t*>(&buffer[offset + 4]), package_descriptor::to_little_endian(2));
  EXPECT_EQ(*reinterpret_cast<uint16_t*>(&buffer[offset + 6]), package_descriptor::to_little_endian(2));
  EXPECT_EQ(*reinterpret_cast<uint16_t*>(&buffer[offset + 8]), package_descriptor::to_little_endian(5));
  EXPECT_EQ(*reinterpret_cast<uint16_t*>(&buffer[offset + 10]), package_descriptor::to_little_endian(1));

  // Verify the mount_point length and value
  offset += 12;
  uint16_t mount_point_length = package_descriptor::to_little_endian(*reinterpret_cast<uint16_t*>(&buffer[offset]));
  EXPECT_EQ(mount_point_length, mount_point.size());
  EXPECT_EQ(std::string(buffer.begin() + offset + 2, buffer.begin() + offset + 2 + mount_point.size()), mount_point);

  // Verify the entry_point length and value
  offset += 2 + mount_point.size();
  uint16_t entry_point_length = package_descriptor::to_little_endian(*reinterpret_cast<uint16_t*>(&buffer[offset]));
  EXPECT_EQ(entry_point_length, entry_point.size());
  EXPECT_EQ(std::string(buffer.begin() + offset + 2, buffer.begin() + offset + 2 + entry_point.size()), entry_point);

  // Verify the cwd indicator
  offset += 2 + entry_point.size();
  EXPECT_EQ(buffer[offset], 0x00);
}

TEST(PackageDescriptorTest, construct_from_parameters)
{
  std::string ruby_version = "3.1.2";
  std::string tebako_version = "2.5.1";
  std::string mount_point = "/app";
  std::string entry_point = "start.rb";
  std::optional<std::string> cwd = "/var/www";

  // Construct the package_descriptor from parameters
  package_descriptor pd(ruby_version, tebako_version, mount_point, entry_point, cwd);

  // Verify the values
  EXPECT_EQ(pd.get_ruby_version_major(), 3);
  EXPECT_EQ(pd.get_ruby_version_minor(), 1);
  EXPECT_EQ(pd.get_ruby_version_patch(), 2);
  EXPECT_EQ(pd.get_tebako_version_major(), 2);
  EXPECT_EQ(pd.get_tebako_version_minor(), 5);
  EXPECT_EQ(pd.get_tebako_version_patch(), 1);
  EXPECT_EQ(pd.get_mount_point(), mount_point);
  EXPECT_EQ(pd.get_entry_point(), entry_point);
  EXPECT_EQ(pd.get_cwd(), cwd);
}

TEST(PackageDescriptorTest, construct_from_parameters_no_cwd)
{
  std::string ruby_version = "3.1.2";
  std::string tebako_version = "2.5.1";
  std::string mount_point = "/app";
  std::string entry_point = "start.rb";
  std::optional<std::string> cwd = std::nullopt;

  // Construct the package_descriptor from parameters
  package_descriptor pd(ruby_version, tebako_version, mount_point, entry_point, cwd);

  // Verify the values
  EXPECT_EQ(pd.get_ruby_version_major(), 3);
  EXPECT_EQ(pd.get_ruby_version_minor(), 1);
  EXPECT_EQ(pd.get_ruby_version_patch(), 2);
  EXPECT_EQ(pd.get_tebako_version_major(), 2);
  EXPECT_EQ(pd.get_tebako_version_minor(), 5);
  EXPECT_EQ(pd.get_tebako_version_patch(), 1);
  EXPECT_EQ(pd.get_mount_point(), mount_point);
  EXPECT_EQ(pd.get_entry_point(), entry_point);
  EXPECT_EQ(pd.get_cwd(), cwd);
}

TEST(PackageDescriptorTest, construct_from_invalid_ruby_version)
{
  std::string invalid_ruby_version = "invalid_version";
  std::string tebako_version = "1.0.0";
  std::string mount_point = "/mnt";
  std::string entry_point = "main.rb";
  std::optional<std::string> cwd = "/home/user";

  // Expect an exception to be thrown due to invalid ruby version
  EXPECT_THROW(package_descriptor pd(invalid_ruby_version, tebako_version, mount_point, entry_point, cwd),
               std::invalid_argument);
}

TEST(PackageDescriptorTest, construct_from_invalid_tebako_version)
{
  std::string invalid_ruby_version = "1.0.0";
  std::string tebako_version = "invalid_version";
  std::string mount_point = "/mnt";
  std::string entry_point = "main.rb";
  std::optional<std::string> cwd = "/home/user";

  // Expect an exception to be thrown due to invalid ruby version
  EXPECT_THROW(package_descriptor pd(invalid_ruby_version, tebako_version, mount_point, entry_point, cwd),
               std::invalid_argument);
}

TEST(PackageDescriptorTest, deserialize_small_buffer_fixed_fields)
{
  // Create a buffer that is too small for fixed-size fields
  std::vector<char> buffer = {
      'T',  'A',  'M', 'A', 'T', 'E', 'B', 'A', 'K', 'O',  // signature

      0x00, 0x01,  // ruby_version_major
      0x00, 0x02,  // ruby_version_minor
                   // Missing the rest of the fixed-size fields
  };

  // Expect an exception to be thrown due to the small buffer
  EXPECT_THROW(package_descriptor pd(buffer), std::out_of_range);
}

TEST(PackageDescriptorTest, deserialize_small_buffer_mount_point)
{
  // Create a buffer that is too small for mount_point
  std::vector<char> buffer = {
      'T',  'A',  'M', 'A', 'T', 'E', 'B', 'A', 'K', 'O',  // signature

      0x00, 0x01,  // ruby_version_major
      0x00, 0x02,  // ruby_version_minor
      0x00, 0x03,  // ruby_version_patch
      0x00, 0x04,  // tebako_version_major
      0x00, 0x05,  // tebako_version_minor
      0x00, 0x06,  // tebako_version_patch
      0x04, 0x00,  // mount_point_size
      'm',  'n'    // Incomplete mount_point
  };

  // Expect an exception to be thrown due to the small buffer
  EXPECT_THROW(package_descriptor pd(buffer), std::out_of_range);
}

TEST(PackageDescriptorTest, deserialize_small_buffer_entry_point)
{
  // Create a buffer that is too small for entry_point
  std::vector<char> buffer = {
      'T',  'A',  'M', 'A', 'T',  'E',  'B', 'A', 'K', 'O',  // signature

      0x00, 0x01,                        // ruby_version_major
      0x00, 0x02,                        // ruby_version_minor
      0x00, 0x03,                        // ruby_version_patch
      0x00, 0x04,                        // tebako_version_major
      0x00, 0x05,                        // tebako_version_minor
      0x00, 0x06,                        // tebako_version_patch
      0x00, 0x04,                        // mount_point_size
      'm',  'n',  't', '/', 0x07, 0x00,  // entry_point_size
      'm',  'a',  'i', 'n', '.',  'r'    // Incomplete entry_point
  };

  // Expect an exception to be thrown due to the small buffer
  EXPECT_THROW(package_descriptor pd(buffer), std::out_of_range);
}

TEST(PackageDescriptorTest, deserialize_small_buffer_cwd)
{
  // Create a buffer that is too small for cwd
  std::vector<char> buffer = {
      'T',  'A',  'M', 'A', 'T',  'E',  'B', 'A', 'K', 'O',  // signature

      0x00, 0x01,                        // ruby_version_major
      0x00, 0x02,                        // ruby_version_minor
      0x00, 0x03,                        // ruby_version_patch
      0x00, 0x04,                        // tebako_version_major
      0x00, 0x05,                        // tebako_version_minor
      0x00, 0x06,                        // tebako_version_patch
      0x00, 0x04,                        // mount_point_size
      'm',  'n',  't', '/', 0x07, 0x00,  // entry_point_size
      'm',  'a',  'i', 'n', '.',  'r',  'b',
      0x01,                                            // cwd_present
      0x0A, 0x00,                                      // cwd_size
      '/',  'h',  'o', 'm', 'e',  '/',  'u', 's', 'e'  // Incomplete cwd
  };

  // Expect an exception to be thrown due to the small buffer
  EXPECT_THROW(package_descriptor pd(buffer), std::out_of_range);
}

TEST(PackageDescriptorTest, construct_from_buffer_wrong_signature)
{
  // Create a buffer with serialized data in big-endian format
  std::vector<char> buffer = {
      'T', 'A', 'M', 'A', 'T', 'E', 'V', 'A', 'K', 'O',  // signature

      0x01, 0x00,  // ruby_version_major
      0x02, 0x00,  // ruby_version_minor
      0x03, 0x00,  // ruby_version_patch
      0x04, 0x00,  // tebako_version_major
      0x05, 0x00,  // tebako_version_minor
      0x06, 0x00,  // tebako_version_patch
      // mount_point
      0x04, 0x00, 'm', 'n', 't', '/',
      // entry_point
      0x07, 0x00, 'm', 'a', 'i', 'n', '.', 'r', 'b',
      // cwd (optional indicator)
      0x00  // cwd not present indicator
  };

  // Convert the buffer to little-endian if necessary
  for (size_t i = 0; i < 6; ++i) {
    uint16_t* value = reinterpret_cast<uint16_t*>(&buffer[i * 2]);
    *value = package_descriptor::to_little_endian(*value);
  }

  // Convert string lengths to little-endian if necessary
  uint16_t* mount_point_length = reinterpret_cast<uint16_t*>(&buffer[12]);
  *mount_point_length = package_descriptor::to_little_endian(*mount_point_length);

  uint16_t* entry_point_length = reinterpret_cast<uint16_t*>(&buffer[18]);
  *entry_point_length = package_descriptor::to_little_endian(*entry_point_length);

  // Expect an exception to be thrown due to the small buffer
  EXPECT_THROW(package_descriptor pd(buffer), std::invalid_argument);
}

}  // namespace tebako