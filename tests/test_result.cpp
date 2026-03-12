/**
 * @file test_result.cpp
 * @brief Unit tests for Result<T, Error> type
 */

#include <gtest/gtest.h>
#include <tebako/fs/core/result.h>
#include <tebako/fs/core/error.h>

using namespace tebako::fs;

// ============================================================================
// Error Tests
// ============================================================================

TEST(ErrorTest, DefaultConstructorIsSuccess)
{
  Error err;
  EXPECT_EQ(err.code, ErrorCode::Success);
  EXPECT_FALSE(err);
  EXPECT_TRUE(err.is_ok());
}

TEST(ErrorTest, ErrorCodeConstructor)
{
  Error err(ErrorCode::NotFound);
  EXPECT_EQ(err.code, ErrorCode::NotFound);
  EXPECT_TRUE(err);
  EXPECT_TRUE(err.is_err());
  EXPECT_EQ(err.message, "Not found");
}

TEST(ErrorTest, FullMessageWithouthContext)
{
  Error err(ErrorCode::NotFound, "File not found");
  EXPECT_EQ(err.full_message(), "File not found");
}

TEST(ErrorTest, FullMessageWithContext)
{
  Error err(ErrorCode::NotFound, "File not found", "/path/to/file");
  EXPECT_EQ(err.full_message(), "File not found: /path/to/file");
}

TEST(ErrorTest, ErrorToString)
{
  EXPECT_STREQ(Error::error_to_string(ErrorCode::Success), "Success");
  EXPECT_STREQ(Error::error_to_string(ErrorCode::NotFound), "Not found");
  EXPECT_STREQ(Error::error_to_string(ErrorCode::IOError), "I/O error");
  EXPECT_STREQ(Error::error_to_string(static_cast<ErrorCode>(999)), "Unknown error");
}

TEST(ErrorTest, MakeNotFoundError)
{
  Error err = make_not_found_error("/test/path");
  EXPECT_EQ(err.code, ErrorCode::NotFound);
  EXPECT_EQ(err.context, "/test/path");
}

// ============================================================================
// Result<T> Tests
// ============================================================================

TEST(ResultTest, DefaultConstructorHasValue)
{
  Result<int> result;
  EXPECT_TRUE(result.is_ok());
  EXPECT_FALSE(result.is_err());
  EXPECT_TRUE(result);
}

TEST(ResultTest, ValueConstructor)
{
  Result<int> result(42);
  EXPECT_TRUE(result.is_ok());
  EXPECT_EQ(result.value(), 42);
}

TEST(ResultTest, ErrConstructor)
{
  Result<int> result(Err{ErrorCode::NotFound});
  EXPECT_TRUE(result.is_err());
  EXPECT_FALSE(result.is_ok());
  EXPECT_EQ(result.error().code, ErrorCode::NotFound);
}

TEST(ResultTest, OkTagConstructor)
{
  Result<int> result(Ok<int>{42});
  EXPECT_TRUE(result.is_ok());
  EXPECT_EQ(result.value(), 42);
}

TEST(ResultTest, UnwrapSuccess)
{
  Result<int> result(42);
  EXPECT_EQ(result.unwrap(), 42);
}

TEST(ResultTest, UnwrapErrorThrows)
{
  Result<int> result(Err{ErrorCode::NotFound});
  EXPECT_THROW(result.unwrap(), BadResultAccess);
}

TEST(ResultTest, UnwrapOrWithSuccess)
{
  Result<int> result(42);
  EXPECT_EQ(result.unwrap_or(0), 42);
}

TEST(ResultTest, UnwrapOrWithError)
{
  Result<int> result(Err{ErrorCode::NotFound});
  EXPECT_EQ(result.unwrap_or(99), 99);
}

TEST(ResultTest, UnwrapOrElseWithSuccess)
{
  Result<int> result(42);
  auto f = [](const Error&) { return 0; };
  EXPECT_EQ(result.unwrap_or_else(f), 42);
}

TEST(ResultTest, UnwrapOrElseWithError)
{
  Result<int> result(Err{ErrorCode::NotFound});
  auto f = [](const Error& err) { return static_cast<int>(err.code); };
  EXPECT_EQ(result.unwrap_or_else(f), 1);  // NotFound = 1
}

TEST(ResultTest, MapSuccess)
{
  Result<int> result(42);
  auto mapped = result.map([](int x) { return x * 2; });
  EXPECT_TRUE(mapped.is_ok());
  EXPECT_EQ(mapped.value(), 84);
}

TEST(ResultTest, MapError)
{
  Result<int> result(Err{ErrorCode::NotFound});
  auto mapped = result.map([](int x) { return x * 2; });
  EXPECT_TRUE(mapped.is_err());
  EXPECT_EQ(mapped.error().code, ErrorCode::NotFound);
}

TEST(ResultTest, AndThenSuccess)
{
  Result<int> result(42);
  auto chained = result.and_then([](int x) -> Result<std::string> { return Ok<std::string>{std::to_string(x)}; });
  EXPECT_TRUE(chained.is_ok());
  EXPECT_EQ(chained.value(), "42");
}

TEST(ResultTest, AndThenError)
{
  Result<int> result(Err{ErrorCode::NotFound});
  auto chained = result.and_then([](int x) -> Result<std::string> { return Ok<std::string>{std::to_string(x)}; });
  EXPECT_TRUE(chained.is_err());
  EXPECT_EQ(chained.error().code, ErrorCode::NotFound);
}

TEST(ResultTest, MapErr)
{
  Result<int> result(Err{ErrorCode::NotFound, "Original"});
  auto mapped =
      std::move(result).map_err([](const Error& e) { return Error(ErrorCode::IOError, "Mapped: " + e.message); });
  EXPECT_TRUE(mapped.is_err());
  EXPECT_EQ(mapped.error().code, ErrorCode::IOError);
  EXPECT_EQ(mapped.error().message, "Mapped: Original");
}

TEST(ResultTest, Equality)
{
  Result<int> a(42);
  Result<int> b(42);
  Result<int> c(43);
  Result<int> d(Err{ErrorCode::NotFound});

  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
  EXPECT_NE(a, d);
}

TEST(ResultTest, MoveSemantics)
{
  Result<std::unique_ptr<int>> result(Ok<std::unique_ptr<int>>{std::make_unique<int>(42)});
  EXPECT_TRUE(result.is_ok());

  auto ptr = std::move(result).unwrap();
  EXPECT_EQ(*ptr, 42);
}

// ============================================================================
// Result<void> Tests
// ============================================================================

TEST(ResultVoidTest, DefaultIsSuccess)
{
  Result<void> result;
  EXPECT_TRUE(result.is_ok());
  EXPECT_TRUE(result);
}

TEST(ResultVoidTest, ErrIsError)
{
  Result<void> result(Err{ErrorCode::NotFound});
  EXPECT_TRUE(result.is_err());
  EXPECT_FALSE(result);
}

TEST(ResultVoidTest, OkVoidTag)
{
  Result<void> result(Ok<void>{});
  EXPECT_TRUE(result.is_ok());
}

TEST(ResultVoidTest, UnwrapSuccess)
{
  Result<void> result;
  EXPECT_NO_THROW(result.unwrap());
}

TEST(ResultVoidTest, UnwrapErrorThrows)
{
  Result<void> result(Err{ErrorCode::NotFound});
  EXPECT_THROW(result.unwrap(), BadResultAccess);
}

TEST(ResultVoidTest, MapErr)
{
  Result<void> result(Err{ErrorCode::NotFound});
  auto mapped = std::move(result).map_err([](const Error& e) { return Error(ErrorCode::IOError, e.message); });
  EXPECT_TRUE(mapped.is_err());
  EXPECT_EQ(mapped.error().code, ErrorCode::IOError);
}

// ============================================================================
// Helper Function Tests
// ============================================================================

TEST(HelperTest, MakeResult)
{
  auto result = make_result(42);
  EXPECT_TRUE(result.is_ok());
  EXPECT_EQ(result.value(), 42);
}

TEST(HelperTest, MakeError)
{
  auto result = make_error(ErrorCode::NotFound);
  EXPECT_TRUE(result.is_err());
  EXPECT_EQ(result.error().code, ErrorCode::NotFound);
}

TEST(HelperTest, MakeErrorWithMessage)
{
  auto result = make_error(ErrorCode::NotFound, "Not found");
  EXPECT_TRUE(result.is_err());
  EXPECT_EQ(result.error().code, ErrorCode::NotFound);
  EXPECT_EQ(result.error().message, "Not found");
}

TEST(HelperTest, MakeOk)
{
  auto result = make_ok();
  EXPECT_TRUE(result.is_ok());
}

// ============================================================================
// Chaining Tests
// ============================================================================

TEST(ChainingTest, MultipleAndThen)
{
  auto result = make_result(10)
                    .and_then([](int x) -> Result<int> { return Ok<int>{x + 5}; })
                    .and_then([](int x) -> Result<int> { return Ok<int>{x * 2}; })
                    .and_then([](int x) -> Result<std::string> { return Ok<std::string>{std::to_string(x)}; });

  EXPECT_TRUE(result.is_ok());
  EXPECT_EQ(result.value(), "30");
}

TEST(ChainingTest, ChainingShortCircuitsOnError)
{
  int side_effect = 0;

  auto result = make_result(10)
                    .and_then([](int) -> Result<int> { return Err{ErrorCode::NotFound}; })
                    .and_then([&](int) -> Result<int> {
                      side_effect = 1;
                      return Ok<int>{0};
                    });

  EXPECT_TRUE(result.is_err());
  EXPECT_EQ(side_effect, 0);  // Never called
}
