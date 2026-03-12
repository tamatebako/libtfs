/**
 * @file test_path.cpp
 * @brief Unit tests for Path class
 */

#include <gtest/gtest.h>
#include <tebako/fs/core/path.h>

using namespace tebako::fs;

// ============================================================================
// Construction and Normalization Tests
// ============================================================================

TEST(PathTest, DefaultConstructorIsEmpty) {
    Path p;
    EXPECT_TRUE(p.empty());
    EXPECT_EQ(p.string(), "");
}

TEST(PathTest, ConstructFromString) {
    Path p("/home/user/file.txt");
    EXPECT_FALSE(p.empty());
    EXPECT_EQ(p.string(), "/home/user/file.txt");
}

TEST(PathTest, ConstructFromStringView) {
    std::string_view sv = "/home/user/file.txt";
    Path p(sv);
    EXPECT_EQ(p.string(), "/home/user/file.txt");
}

TEST(PathTest, ConstructFromCString) {
    Path p("/home/user/file.txt");
    EXPECT_EQ(p.string(), "/home/user/file.txt");
}

TEST(PathTest, ConstructFromNullCString) {
    Path p(static_cast<const char*>(nullptr));
    EXPECT_TRUE(p.empty());
}

// ============================================================================
// Normalization Tests
// ============================================================================

TEST(PathNormalizationTest, RemoveLeadingDotSlash) {
    EXPECT_EQ(Path("./file.txt").string(), "file.txt");
    EXPECT_EQ(Path("./home/user").string(), "home/user");
}

TEST(PathNormalizationTest, RemoveEmbeddedDotSlash) {
    EXPECT_EQ(Path("/home/./user").string(), "/home/user");
    EXPECT_EQ(Path("/./home/./user").string(), "/home/user");
}

TEST(PathNormalizationTest, RemoveTrailingSlash) {
    EXPECT_EQ(Path("/home/user/").string(), "/home/user");
    EXPECT_EQ(Path("/home/").string(), "/home");
}

TEST(PathNormalizationTest, PreserveRootSlash) {
    EXPECT_EQ(Path("/").string(), "/");
}

TEST(PathNormalizationTest, CollapseMultipleSlashes) {
    EXPECT_EQ(Path("//home//user").string(), "/home/user");
    EXPECT_EQ(Path("/home///file").string(), "/home/file");
}

TEST(PathNormalizationTest, HandleTrailingDot) {
    EXPECT_EQ(Path("/home/user/.").string(), "/home/user");
    EXPECT_EQ(Path("/.").string(), "/");
}

TEST(PathNormalizationTest, ResolveParentDotDot) {
    EXPECT_EQ(Path("/home/user/../other").string(), "/home/other");
    EXPECT_EQ(Path("/home/user/..").string(), "/home");
}

TEST(PathNormalizationTest, ComplexNormalization) {
    EXPECT_EQ(Path("/home/./user/../other/./file/").string(), "/home/other/file");
}

// ============================================================================
// Property Tests
// ============================================================================

TEST(PathPropertyTest, IsAbsolute) {
    EXPECT_TRUE(Path("/home/user").is_absolute());
    EXPECT_TRUE(Path("/").is_absolute());
    EXPECT_FALSE(Path("home/user").is_absolute());
    EXPECT_FALSE(Path().is_absolute());
}

TEST(PathPropertyTest, IsRelative) {
    EXPECT_TRUE(Path("home/user").is_relative());
    EXPECT_FALSE(Path("/home/user").is_relative());
    EXPECT_FALSE(Path("/").is_relative());
    EXPECT_FALSE(Path().is_relative());
}

TEST(PathPropertyTest, IsRoot) {
    EXPECT_TRUE(Path("/").is_root());
    EXPECT_FALSE(Path("/home").is_root());
    EXPECT_FALSE(Path().is_root());
}

TEST(PathPropertyTest, Length) {
    EXPECT_EQ(Path("/home/user").length(), 10);
    EXPECT_EQ(Path().length(), 0);
    EXPECT_EQ(Path("/").length(), 1);
}

// ============================================================================
// Component Extraction Tests
// ============================================================================

TEST(PathComponentTest, Parent) {
    EXPECT_EQ(Path("/home/user/file.txt").parent().string(), "/home/user");
    EXPECT_EQ(Path("/home/user/").parent().string(), "/home");
    EXPECT_EQ(Path("/home").parent().string(), "/");
    EXPECT_EQ(Path("/").parent().string(), "");
    EXPECT_EQ(Path("file.txt").parent().string(), "");
    EXPECT_EQ(Path().parent().string(), "");
}

TEST(PathComponentTest, Filename) {
    EXPECT_EQ(Path("/home/user/file.txt").filename(), "file.txt");
    EXPECT_EQ(Path("/home/user/").filename(), "user");
    EXPECT_EQ(Path("/home").filename(), "home");
    EXPECT_EQ(Path("/").filename(), "");
    EXPECT_EQ(Path("file.txt").filename(), "file.txt");
}

TEST(PathComponentTest, Extension) {
    EXPECT_EQ(Path("/home/file.txt").extension(), ".txt");
    EXPECT_EQ(Path("/home/file.TAR.GZ").extension(), ".GZ");
    EXPECT_EQ(Path("/home/file").extension(), "");
    EXPECT_EQ(Path("/home/.hidden").extension(), "");  // Hidden file, not extension
    EXPECT_EQ(Path("/").extension(), "");
}

TEST(PathComponentTest, Stem) {
    EXPECT_EQ(Path("/home/file.txt").stem(), "file");
    EXPECT_EQ(Path("/home/file").stem(), "file");
    EXPECT_EQ(Path("/home/.hidden").stem(), ".hidden");  // Hidden file
    EXPECT_EQ(Path("/").stem(), "");
}

// ============================================================================
// Join Tests
// ============================================================================

TEST(PathJoinTest, JoinRelative) {
    EXPECT_EQ(Path("/home").join("user").string(), "/home/user");
    EXPECT_EQ(Path("/home").join(Path("user")).string(), "/home/user");
}

TEST(PathJoinTest, JoinEmpty) {
    EXPECT_EQ(Path("/home").join("").string(), "/home");
    EXPECT_EQ(Path().join("home").string(), "home");
}

TEST(PathJoinTest, JoinAbsolute) {
    // Absolute path gets converted to relative when joining
    EXPECT_EQ(Path("/home").join("/user").string(), "/home/user");
}

TEST(PathJoinTest, ChainJoin) {
    Path p = Path("/").join("home").join("user").join("file.txt");
    EXPECT_EQ(p.string(), "/home/user/file.txt");
}

// ============================================================================
// Relative To Tests
// ============================================================================

TEST(PathRelativeTest, BasicRelative) {
    EXPECT_EQ(Path::relative_to("/home/user/file", "/home").string(), "user/file");
    EXPECT_EQ(Path::relative_to("/home/user/file", "/home/").string(), "user/file");
}

TEST(PathRelativeTest, SameDirectory) {
    EXPECT_EQ(Path::relative_to("/home/user", "/home/user").string(), "");
}

TEST(PathRelativeTest, NotUnderBase) {
    EXPECT_EQ(Path::relative_to("/other/path", "/home").string(), "");
}

TEST(PathRelativeTest, RelativePaths) {
    EXPECT_EQ(Path::relative_to("user/file", "user").string(), "file");
}

// ============================================================================
// Backend Utility Tests
// ============================================================================

TEST(PathUtilityTest, WithoutLeadingSlash) {
    EXPECT_EQ(Path("/home/user").without_leading_slash(), "home/user");
    EXPECT_EQ(Path("home/user").without_leading_slash(), "home/user");
    EXPECT_EQ(Path("/").without_leading_slash(), "");
    EXPECT_EQ(Path().without_leading_slash(), "");
}

TEST(PathUtilityTest, WithTrailingSlash) {
    EXPECT_EQ(Path("/home/user").with_trailing_slash(), "/home/user/");
    EXPECT_EQ(Path("/home/user/").with_trailing_slash(), "/home/user/");
    EXPECT_EQ(Path("/").with_trailing_slash(), "/");
    EXPECT_EQ(Path().with_trailing_slash(), "/");
}

TEST(PathUtilityTest, HasExtension) {
    EXPECT_TRUE(Path("file.txt").has_extension(".txt"));
    EXPECT_TRUE(Path("file.txt").has_extension("txt"));  // Without dot
    EXPECT_TRUE(Path("file.TXT").has_extension(".txt")); // Case insensitive
    EXPECT_FALSE(Path("file.txt").has_extension(".csv"));
    EXPECT_FALSE(Path("file").has_extension(".txt"));
    EXPECT_FALSE(Path(".hidden").has_extension(".hidden"));
}

// ============================================================================
// Comparison Tests
// ============================================================================

TEST(PathComparisonTest, Equality) {
    EXPECT_EQ(Path("/home/user"), Path("/home/user"));
    EXPECT_NE(Path("/home/user"), Path("/home/other"));
    EXPECT_EQ(Path("/home/user/"), Path("/home/user"));  // Normalized
}

TEST(PathComparisonTest, LessThan) {
    EXPECT_LT(Path("/home/a"), Path("/home/b"));
    EXPECT_FALSE(Path("/home/b") < Path("/home/a"));
}

// ============================================================================
// String Access Tests
// ============================================================================

TEST(PathStringTest, StringAccess) {
    Path p("/home/user");
    EXPECT_EQ(p.string(), "/home/user");
    EXPECT_STREQ(p.c_str(), "/home/user");
    EXPECT_EQ(p.view(), "/home/user");
}

TEST(PathStringTest, StringViewConversion) {
    Path p("/home/user");
    std::string_view sv = p;
    EXPECT_EQ(sv, "/home/user");
}
