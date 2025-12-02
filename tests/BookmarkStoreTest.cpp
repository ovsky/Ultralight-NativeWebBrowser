#include <gtest/gtest.h>

#include "BookmarkStore.h"

#include <filesystem>
#include <fstream>
#include <random>

namespace
{
  std::filesystem::path MakeTempPath()
  {
    auto base = std::filesystem::temp_directory_path();
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;
    std::filesystem::path candidate;
    do
    {
      candidate = base / (std::string("ultralight_bookmarks_") + std::to_string(dist(gen)) + ".json");
    } while (std::filesystem::exists(candidate));
    return candidate;
  }
}

TEST(BookmarkStoreTest, CreatesDefaultRoots)
{
  auto temp_path = MakeTempPath();
  BookmarkStore store;
  ASSERT_TRUE(store.LoadOrCreate(temp_path));
  EXPECT_NE(store.bookmarks_bar_id(), 0u);
  EXPECT_NE(store.other_bookmarks_id(), 0u);
  auto bar = store.GetNode(store.bookmarks_bar_id());
  auto other = store.GetNode(store.other_bookmarks_id());
  ASSERT_NE(bar, nullptr);
  ASSERT_NE(other, nullptr);
  EXPECT_EQ(bar->type, BookmarkStore::NodeType::Folder);
  EXPECT_EQ(other->type, BookmarkStore::NodeType::Folder);
  std::filesystem::remove(temp_path);
}

TEST(BookmarkStoreTest, AddBookmarkPersistsAcrossReload)
{
  auto temp_path = MakeTempPath();
  BookmarkStore store;
  ASSERT_TRUE(store.LoadOrCreate(temp_path));
  uint64_t parent = store.bookmarks_bar_id();
  ASSERT_NE(parent, 0u);
  auto bookmark_id = store.AddBookmark("Example", "https://example.com", parent, 0);
  ASSERT_NE(bookmark_id, 0u);

  BookmarkStore reloaded;
  ASSERT_TRUE(reloaded.LoadOrCreate(temp_path));
  auto node = reloaded.GetNode(bookmark_id);
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->url, "https://example.com");
  EXPECT_EQ(node->title, "Example");
  EXPECT_EQ(node->parent_id, parent);

  std::filesystem::remove(temp_path);
}
