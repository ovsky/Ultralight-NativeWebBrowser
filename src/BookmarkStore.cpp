#include "BookmarkStore.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stack>

#include <nlohmann/json.hpp>

namespace
{
  std::string TrimCopy(const std::string &text)
  {
    size_t start = 0;
    size_t end = text.size();
    while (start < end && std::isspace(static_cast<unsigned char>(text[start])))
      ++start;
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])))
      --end;
    return text.substr(start, end - start);
  }

  std::string NormalizeUrl(const std::string &url)
  {
    std::string trimmed = TrimCopy(url);
    std::string result = trimmed;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
    return result;
  }
}

BookmarkStore::BookmarkStore()
{
  EnsureDefaultRoots();
}

bool BookmarkStore::LoadOrCreate(const std::filesystem::path &path)
{
  storage_path_ = path;
  namespace fs = std::filesystem;
  std::error_code ec;
  if (!path.parent_path().empty())
    fs::create_directories(path.parent_path(), ec);

  if (!fs::exists(path))
  {
    nodes_.clear();
    next_id_ = 1;
    EnsureDefaultRoots();
    return Save();
  }

  std::ifstream in(path, std::ios::in | std::ios::binary);
  if (!in.is_open())
  {
    nodes_.clear();
    next_id_ = 1;
    EnsureDefaultRoots();
    return false;
  }

  nlohmann::json doc;
  try
  {
    in >> doc;
  }
  catch (...)
  {
    nodes_.clear();
    next_id_ = 1;
    EnsureDefaultRoots();
    return false;
  }

  nodes_.clear();
  next_id_ = doc.value("next_id", 1ull);
  auto roots = doc.value("roots", nlohmann::json::object());
  bar_root_id_ = roots.value("bookmarks_bar", 0ull);
  other_root_id_ = roots.value("other_bookmarks", 0ull);

  auto nodes_json = doc.value("nodes", nlohmann::json::array());
  for (const auto &item : nodes_json)
  {
    Node node;
    node.id = item.value("id", 0ull);
    const std::string type_str = item.value("type", std::string("bookmark"));
    node.type = NodeTypeFromString(type_str);
    node.title = item.value("title", std::string());
    node.url = item.value("url", std::string());
    node.parent_id = item.value("parent_id", 0ull);
    node.date_added = item.value("date_added", 0ull);
    node.date_modified = item.value("date_modified", node.date_added);
    node.children = item.value("children", std::vector<uint64_t>());
    nodes_[node.id] = std::move(node);
  }

  if (!nodes_.count(bar_root_id_) || !nodes_.count(other_root_id_))
  {
    nodes_.clear();
    next_id_ = 1;
    EnsureDefaultRoots();
    Save();
    return false;
  }

  return true;
}

bool BookmarkStore::Save()
{
  if (storage_path_.empty())
    return false;

  EnsureDefaultRoots();

  nlohmann::json doc;
  doc["version"] = 1;
  doc["next_id"] = next_id_;
  doc["roots"] = {
      {"bookmarks_bar", bar_root_id_},
      {"other_bookmarks", other_root_id_}};

  nlohmann::json nodes_json = nlohmann::json::array();
  for (const auto &entry : nodes_)
  {
    const Node &node = entry.second;
    nlohmann::json item;
    item["id"] = node.id;
    item["type"] = NodeTypeToString(node.type);
    item["title"] = node.title;
    item["url"] = node.url;
    item["parent_id"] = node.parent_id;
    item["date_added"] = node.date_added;
    item["date_modified"] = node.date_modified;
    item["children"] = node.children;
    nodes_json.push_back(item);
  }
  doc["nodes"] = nodes_json;

  std::ofstream out(storage_path_, std::ios::out | std::ios::binary | std::ios::trunc);
  if (!out.is_open())
    return false;
  out << doc.dump(2);
  out.flush();
  return out.good();
}

const BookmarkStore::Node *BookmarkStore::GetNode(uint64_t id) const
{
  auto it = nodes_.find(id);
  if (it == nodes_.end())
    return nullptr;
  return &it->second;
}

BookmarkStore::Node *BookmarkStore::MutableNode(uint64_t id)
{
  auto it = nodes_.find(id);
  if (it == nodes_.end())
    return nullptr;
  return &it->second;
}

std::vector<const BookmarkStore::Node *> BookmarkStore::GetChildren(uint64_t parent_id) const
{
  std::vector<const Node *> result;
  auto parent = GetNode(parent_id);
  if (!parent)
    return result;
  result.reserve(parent->children.size());
  for (auto child_id : parent->children)
  {
    auto child = GetNode(child_id);
    if (child)
      result.push_back(child);
  }
  return result;
}

uint64_t BookmarkStore::AddBookmark(const std::string &title, const std::string &url,
                                    uint64_t parent_id, size_t index)
{
  std::string normalized_url = TrimCopy(url);
  if (normalized_url.empty())
    return 0;

  auto parent = MutableNode(parent_id);
  if (!parent || parent->type != NodeType::Folder)
    parent = MutableNode(bookmarks_bar_id());
  if (!parent)
    return 0;

  Node node;
  node.id = NextId();
  node.type = NodeType::Bookmark;
  node.title = title.empty() ? normalized_url : title;
  node.url = normalized_url;
  node.parent_id = parent->id;
  node.date_added = node.date_modified = NowMs();
  nodes_[node.id] = node;
  InsertChild(parent->id, node.id, index);
  if (!Save())
    std::cerr << "Failed to save bookmarks after AddBookmark" << std::endl;
  NotifyChanged();
  return node.id;
}

uint64_t BookmarkStore::AddFolder(const std::string &title, uint64_t parent_id, size_t index)
{
  auto parent = MutableNode(parent_id);
  if (!parent || parent->type != NodeType::Folder)
    parent = MutableNode(bookmarks_bar_id());
  if (!parent)
    return 0;

  Node node;
  node.id = NextId();
  node.type = NodeType::Folder;
  node.title = title.empty() ? "New folder" : title;
  node.parent_id = parent->id;
  node.date_added = node.date_modified = NowMs();
  nodes_[node.id] = node;
  InsertChild(parent->id, node.id, index);
  if (!Save())
    std::cerr << "Failed to save bookmarks after AddFolder" << std::endl;
  NotifyChanged();
  return node.id;
}

bool BookmarkStore::UpdateBookmark(uint64_t id, const std::string &title, const std::string &url)
{
  auto node = MutableNode(id);
  if (!node || node->type != NodeType::Bookmark)
    return false;
  node->title = title.empty() ? node->title : title;
  if (!url.empty())
    node->url = TrimCopy(url);
  TouchNode(*node);
  bool ok = Save();
  if (!ok)
    std::cerr << "Failed to save bookmarks after UpdateBookmark" << std::endl;
  NotifyChanged();
  return ok;
}

bool BookmarkStore::UpdateFolder(uint64_t id, const std::string &title)
{
  auto node = MutableNode(id);
  if (!node || node->type != NodeType::Folder)
    return false;
  node->title = title;
  TouchNode(*node);
  bool ok = Save();
  if (!ok)
    std::cerr << "Failed to save bookmarks after UpdateFolder" << std::endl;
  NotifyChanged();
  return ok;
}

bool BookmarkStore::InsertChild(uint64_t parent_id, uint64_t child_id, size_t index)
{
  auto parent = MutableNode(parent_id);
  if (!parent || parent->type != NodeType::Folder)
    return false;
  if (index > parent->children.size())
    index = parent->children.size();
  parent->children.insert(parent->children.begin() + index, child_id);
  TouchNode(*parent);
  return true;
}

bool BookmarkStore::RemoveChild(uint64_t parent_id, uint64_t child_id)
{
  auto parent = MutableNode(parent_id);
  if (!parent)
    return false;
  auto it = std::find(parent->children.begin(), parent->children.end(), child_id);
  if (it == parent->children.end())
    return false;
  parent->children.erase(it);
  TouchNode(*parent);
  return true;
}

bool BookmarkStore::MoveNode(uint64_t id, uint64_t new_parent_id, size_t new_index)
{
  if (id == bar_root_id_ || id == other_root_id_)
    return false;

  auto node = MutableNode(id);
  auto new_parent = MutableNode(new_parent_id);
  if (!node)
    return false;
  if (!new_parent || new_parent->type != NodeType::Folder)
    new_parent = MutableNode(bookmarks_bar_id());
  if (!new_parent)
    return false;
  if (IsAncestor(id, new_parent->id))
    return false;

  auto old_parent = MutableNode(node->parent_id);
  if (old_parent)
    RemoveChild(old_parent->id, node->id);

  node->parent_id = new_parent->id;
  InsertChild(new_parent->id, node->id, new_index);
  TouchNode(*node);
  bool ok = Save();
  if (!ok)
    std::cerr << "Failed to save bookmarks after MoveNode" << std::endl;
  NotifyChanged();
  return ok;
}

bool BookmarkStore::RemoveNode(uint64_t id)
{
  if (id == bar_root_id_ || id == other_root_id_)
    return false;

  auto node = GetNode(id);
  if (!node)
    return false;

  auto parent = MutableNode(node->parent_id);
  if (parent)
    RemoveChild(parent->id, id);

  DeleteRecursive(id);
  bool ok = Save();
  if (!ok)
    std::cerr << "Failed to save bookmarks after RemoveNode" << std::endl;
  NotifyChanged();
  return ok;
}

void BookmarkStore::DeleteRecursive(uint64_t id)
{
  auto node = MutableNode(id);
  if (!node)
    return;
  if (node->type == NodeType::Folder)
  {
    auto children = node->children;
    for (auto child_id : children)
      DeleteRecursive(child_id);
  }
  nodes_.erase(id);
}

bool BookmarkStore::IsUrlBookmarked(const std::string &url, uint64_t *bookmark_id) const
{
  if (url.empty())
    return false;
  std::string normalized = NormalizeUrl(url);
  for (const auto &entry : nodes_)
  {
    const Node &node = entry.second;
    if (node.type != NodeType::Bookmark)
      continue;
    if (NormalizeUrl(node.url) == normalized)
    {
      if (bookmark_id)
        *bookmark_id = node.id;
      return true;
    }
  }
  return false;
}

std::string BookmarkStore::SerializeTree(uint64_t root_id) const
{
  std::function<nlohmann::json(uint64_t)> builder = [&](uint64_t id) -> nlohmann::json {
    auto node = GetNode(id);
    if (!node)
      return nlohmann::json::object();
    nlohmann::json obj;
    obj["id"] = node->id;
    obj["title"] = node->title;
    obj["type"] = NodeTypeToString(node->type);
    obj["url"] = node->url;
    obj["parentId"] = node->parent_id;
    obj["dateAdded"] = node->date_added;
    obj["dateModified"] = node->date_modified;
    obj["children"] = nlohmann::json::array();
    for (auto child_id : node->children)
    {
      auto child_json = builder(child_id);
      if (!child_json.is_null())
        obj["children"].push_back(child_json);
    }
    return obj;
  };

  nlohmann::json tree = builder(root_id);
  if (tree.is_null())
    tree = nlohmann::json::object();
  return tree.dump();
}

std::string BookmarkStore::SerializeFoldersList() const
{
  std::vector<uint64_t> folder_ids;
  CollectFolderIds(folder_ids);
  nlohmann::json arr = nlohmann::json::array();
  for (auto id : folder_ids)
  {
    auto node = GetNode(id);
    if (!node)
      continue;
    nlohmann::json item;
    item["id"] = node->id;
    item["title"] = node->title;
    item["path"] = BuildFolderPath(id);
    item["parentId"] = node->parent_id;
    arr.push_back(item);
  }
  return arr.dump();
}

void BookmarkStore::EnsureDefaultRoots()
{
  if (nodes_.count(bar_root_id_) && nodes_.count(other_root_id_))
    return;

  nodes_.clear();
  uint64_t now = NowMs();
  bar_root_id_ = NextId();
  Node bar;
  bar.id = bar_root_id_;
  bar.type = NodeType::Folder;
  bar.title = "Bookmarks bar";
  bar.parent_id = 0;
  bar.date_added = bar.date_modified = now;
  nodes_[bar.id] = bar;

  other_root_id_ = NextId();
  Node other;
  other.id = other_root_id_;
  other.type = NodeType::Folder;
  other.title = "Other bookmarks";
  other.parent_id = 0;
  other.date_added = other.date_modified = now;
  nodes_[other.id] = other;
}

uint64_t BookmarkStore::NextId()
{
  return next_id_++;
}

uint64_t BookmarkStore::NowMs() const
{
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::system_clock::now().time_since_epoch())
                                   .count());
}

void BookmarkStore::TouchNode(Node &node)
{
  node.date_modified = NowMs();
}

bool BookmarkStore::IsAncestor(uint64_t ancestor_id, uint64_t descendant_id) const
{
  auto current = GetNode(descendant_id);
  while (current)
  {
    if (current->parent_id == ancestor_id)
      return true;
    current = GetNode(current->parent_id);
  }
  return false;
}

std::string BookmarkStore::BuildFolderPath(uint64_t id) const
{
  std::vector<std::string> parts;
  auto current = GetNode(id);
  while (current)
  {
    parts.push_back(current->title.empty() ? std::string("(untitled)") : current->title);
    if (current->parent_id == 0)
      break;
    current = GetNode(current->parent_id);
  }
  std::ostringstream ss;
  for (size_t i = 0; i < parts.size(); ++i)
  {
    if (i > 0)
      ss << " / ";
    ss << parts[parts.size() - 1 - i];
  }
  return ss.str();
}

void BookmarkStore::CollectFolderIds(std::vector<uint64_t> &out) const
{
  for (const auto &entry : nodes_)
  {
    if (entry.second.type == NodeType::Folder)
      out.push_back(entry.first);
  }
  std::sort(out.begin(), out.end());
}

std::string BookmarkStore::NodeTypeToString(NodeType type)
{
  return type == NodeType::Folder ? "folder" : "bookmark";
}

BookmarkStore::NodeType BookmarkStore::NodeTypeFromString(const std::string &value)
{
  return value == "folder" ? NodeType::Folder : NodeType::Bookmark;
}

void BookmarkStore::NotifyChanged()
{
  if (on_changed_)
    on_changed_();
}
