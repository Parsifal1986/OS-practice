#define FUSE_USE_VERSION 34
#include <fuse3/fuse.h>
#include <cstring>
#include <mutex>
#include <map>

namespace gptfs {
  struct Session {
    std::string input;
    std::string output;
    std::string error;
  };

  static std::map<std::string, Session> sessions;

  int gptfs_mkdir(const char *path, mode_t mode) {
    if (strncmp(path, "/", 1) != 0) {
      return -ENOENT;
    }
    std::string sessionName = path + 1;
    sessions[sessionName] = Session();
    return 0;
  }

  std::string generate_reply(const std::string &prompt) {
    return std::string("You said: ") + prompt;
  }

  int gptfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
      stbuf->st_mode = S_IFDIR | 0755;
      stbuf->st_nlink = 2;
    } else {
      std::string fullPath(path + 1);
      size_t delim = fullPath.find('/');
      std::string sessionName = (delim == std::string::npos ? fullPath : fullPath.substr(0, delim));
      auto it = sessions.find(sessionName);
      if (it == sessions.end()) {
        return -ENOENT;
      }
      if (delim == std::string::npos) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
      } else {
        std::string fileName = fullPath.substr(delim + 1);
        if (fileName=="input" || fileName=="output" || fileName=="error") {
          stbuf->st_mode = S_IFREG | 0666;
          stbuf->st_nlink = 1;
          const std::string &content = (fileName=="input"? it->second.input : fileName=="output"? it->second.output : it->second.error);
          stbuf->st_size = content.size();
        } else {
          return -ENOENT;
        }
      }
    }
    return 0;
  }

  int gptfs_open(const char *path, struct fuse_file_info *fi) {
    struct stat st;
    if (gptfs_getattr(path, &st, fi) == -ENOENT) {
      return -ENOENT;
    }
    
    return 0;
  }

  int gptfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    std::string fullPath(path + 1);
    size_t delim = fullPath.find('/');
    if (delim == std::string::npos) return -EISDIR;
    std::string sessionName = fullPath.substr(0, delim);
    std::string fileName = fullPath.substr(delim + 1);
    auto it = sessions.find(sessionName);
    if (it == sessions.end()) return -ENOENT;
    std::string content;
    if (fileName == "input") content = it->second.input;
    else if (fileName == "output") content = it->second.output;
    else if (fileName == "error") content = it->second.error;
    else return -ENOENT;
    size_t len = content.size();
    if ((size_t)offset >= len) {
        return 0;
    }
    size_t bytes = std::min(size, len - (size_t)offset);
    memcpy(buf, content.c_str() + offset, bytes);
    return bytes;
  }

  int gptfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
    struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    if (strcmp(path, "/") == 0) {
      filler(buf, ".", NULL, 0, FUSE_FILL_DIR_PLUS);
      filler(buf, "..", NULL, 0, FUSE_FILL_DIR_PLUS);
      for (auto &entry : sessions) {
        filler(buf, entry.first.c_str(), NULL, 0, FUSE_FILL_DIR_PLUS);
      }
    } else {
        std::string sessionName = path + 1;
        auto it = sessions.find(sessionName);
        if (it == sessions.end()) return -ENOENT;
        filler(buf, ".", NULL, 0, FUSE_FILL_DIR_PLUS);
        filler(buf, "..", NULL, 0, FUSE_FILL_DIR_PLUS);
        filler(buf, "input", NULL, 0, FUSE_FILL_DIR_PLUS);
        filler(buf, "output", NULL, 0, FUSE_FILL_DIR_PLUS);
        filler(buf, "error", NULL, 0, FUSE_FILL_DIR_PLUS);
    }
    return 0;
  }

  int gptfs_write(const char *path, const char *buf, size_t size, off_t offset,
    struct fuse_file_info *fi) {
    std::string fullPath(path + 1);
    size_t delim = fullPath.find('/');
    if (delim == std::string::npos) return -EIO;
    std::string sessionName = fullPath.substr(0, delim);
    std::string fileName = fullPath.substr(delim + 1);
    auto it = sessions.find(sessionName);
    if (it == sessions.end()) return -ENOENT;
    if (fileName == "input") {
        std::string &inputStr = it->second.input;
        if (offset == 0) {
            inputStr.clear();
        }
        inputStr.insert(offset, buf, size);
        return size;
    }
    return -EACCES;
  }

  int gptfs_release(const char *path, struct fuse_file_info *fi) {
    std::string p(path);
    if (p.rfind("/input") == p.size() - 6) {
        std::string sessionName = p.substr(1, p.size() - 1 - 6);
        auto it = sessions.find(sessionName);
        if (it != sessions.end()) {
            std::string prompt = it->second.input;
            std::string reply;
            try {
                reply = generate_reply(prompt);
                it->second.output = reply;
                it->second.error.clear();
            } catch (const std::exception &e) {
                it->second.output.clear();
                it->second.error = std::string("Error: ") + e.what();
            }
        }
    }
    return 0;
  }

  static struct fuse_operations gptfs_ops = {
    .getattr = gptfs_getattr,
    .mkdir = gptfs_mkdir,
    .open = gptfs_open,
    .read = gptfs_read,
    .write = gptfs_write,
    .release = gptfs_release,
    .readdir = gptfs_readdir,
  };

} // namespace gptfs

int main(int argc, char *argv[]){
  return fuse_main(argc, argv, &gptfs::gptfs_ops, NULL);
}