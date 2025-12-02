#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>

class BookmarkStore
{
public:
  enum class NodeType
  {
    Bookmark,
    Folder
  };

  struct Node
  {
    uint64_t id = 0;
    NodeType type = NodeType::Bookmark;
    std::string title;
    std::string url;
    uint64_t parent_id = 0;
    std::vector<uint64_t> children;
    uint64_t date_added = 0;
    uint64_t date_modified = 0;
  };

  using ChangeCallback = std::function<void()>;

  BookmarkStore();

  bool LoadOrCreate(const std::filesystem::path &path);
  bool Save();

  uint64_t bookmarks_bar_id() const { return bar_root_id_; }
  uint64_t other_bookmarks_id() const { return other_root_id_; }

  const Node *GetNode(uint64_t id) const;
  std::vector<const Node *> GetChildren(uint64_t parent_id) const;

  uint64_t AddBookmark(const std::string &title, const std::string &url,
                       uint64_t parent_id, size_t index);
  uint64_t AddFolder(const std::string &title, uint64_t parent_id, size_t index);

  bool UpdateBookmark(uint64_t id, const std::string &title, const std::string &url);
  bool UpdateFolder(uint64_t id, const std::string &title);
  bool MoveNode(uint64_t id, uint64_t new_parent_id, size_t new_index);
  bool RemoveNode(uint64_t id);

  bool IsUrlBookmarked(const std::string &url, uint64_t *bookmark_id = nullptr) const;

  std::string SerializeTree(uint64_t root_id) const;
  std::string SerializeFoldersList() const;

  void SetOnChanged(ChangeCallback cb) { on_changed_ = std::move(cb); }

private:
  Node *MutableNode(uint64_t id);
  void EnsureDefaultRoots();
  uint64_t NextId();
  uint64_t NowMs() const;
  void TouchNode(Node &node);
  bool InsertChild(uint64_t parent_id, uint64_t child_id, size_t index);
  bool RemoveChild(uint64_t parent_id, uint64_t child_id);
  void DeleteRecursive(uint64_t id);
  bool IsAncestor(uint64_t ancestor_id, uint64_t descendant_id) const;
  std::string BuildFolderPath(uint64_t id) const;
  void CollectFolderIds(std::vector<uint64_t> &out) const;
  static std::string NodeTypeToString(NodeType type);
  static NodeType NodeTypeFromString(const std::string &value);
  void NotifyChanged();

  std::unordered_map<uint64_t, Node> nodes_;
  uint64_t next_id_ = 1;
  uint64_t bar_root_id_ = 0;
  uint64_t other_root_id_ = 0;
  std::filesystem::path storage_path_;
  ChangeCallback on_changed_;
};
