/**
 * @file tebakofs_main.cpp
 * @brief Main entry point for tebakofs CLI tool
 *
 * Copyright (c) 2021-2025 Ribose Inc.
 * All rights reserved.
 */

#include <tebako/fs/cli/tebakofs.h>
#include <iostream>

int main(int argc, char* argv[])
{
  try {
    tebako::fs::cli::TebakofsCLI cli;
    return cli.run(argc, argv);
  }
  catch (const std::exception& e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  }
}