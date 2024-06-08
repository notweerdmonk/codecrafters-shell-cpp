#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <getopt.h>
#include <dirent.h>
#include <limits.h>

/* String convenience */

template<typename CharT = char,
         typename Traits = std::char_traits<CharT>,
         typename Allocator = std::allocator<CharT>>
std::size_t rstrip(std::basic_string<CharT, Traits, Allocator>& str,
    const std::basic_string<CharT, Traits, Allocator>& chars) {
  std::size_t count = 0;
  std::size_t endpos = str.find_last_not_of(chars);
  if (endpos != std::basic_string<CharT>::npos) {
    count = str.size() - endpos - 1;
    str.erase(endpos + 1);
  } else {
    count = str.size();
    str.clear();
  }
  return count;
}

template<typename CharT = char,
         typename Traits = std::char_traits<CharT>,
         typename Allocator = std::allocator<CharT>>
std::size_t rstrip(std::basic_string<CharT, Traits, Allocator>& str,
    const char* chars) {
  std::basic_string<CharT, Traits, Allocator> charstr(chars);
  std::size_t count = 0;
  std::size_t endpos = str.find_last_not_of(charstr);
  if (endpos != std::basic_string<CharT>::npos) {
    count = str.size() - endpos - 1;
    str.erase(endpos + 1);
  } else {
    count = str.size();
    str.clear();
  }
  return count;
}

template<typename CharT = char,
         typename Traits = std::char_traits<CharT>,
         typename Allocator = std::allocator<CharT>>
std::size_t lstrip(std::basic_string<CharT, Traits, Allocator>& str,
    const std::basic_string<CharT, Traits, Allocator>& chars) {
  std::size_t original_size = str.size();
  std::size_t new_start = str.find_first_not_of(chars);
  if (new_start == std::basic_string<CharT>::npos) {
    str.clear();
    return original_size;
  } else {
    str.erase(0, new_start);
    return new_start;
  }
}

template<typename CharT = char,
         typename Traits = std::char_traits<CharT>,
         typename Allocator = std::allocator<CharT>>
std::size_t lstrip(std::basic_string<CharT, Traits, Allocator>& str,
    const char* chars) {
  std::basic_string<CharT, Traits, Allocator> charstr(chars);
  std::size_t original_size = str.size();
  std::size_t new_start = str.find_first_not_of(charstr);
  if (new_start == std::basic_string<CharT>::npos) {
    str.clear();
    return original_size;
  } else {
    str.erase(0, new_start);
    return new_start;
  }
}

/* Functions */
void tokenize(const std::string &line, int &argc, char **&argv) {
  std::istringstream iss(line);
  std::string token;
  std::vector<std::string> tokens;

  while (std::getline(iss, token, ' ')) {
    if (!lstrip<>(token, "\"")) {
      lstrip<>(token, "'");
    }
    if (!rstrip<>(token, "\"")) {
      rstrip<>(token, "'");
    }

    tokens.push_back(token);
  }

  argv = (char**)malloc(tokens.size() * sizeof(char*));
  if (!argv) {
    std::cerr << "Malloc failed: " << strerror(errno) << '\n';
    return;
  }

  std::size_t i = 0;
  bool failed = false;
  for (auto &token : tokens) {
    argv[i] = (char*)malloc(token.length() * sizeof(char));
    if (!argv[i]) {
      std::cerr << "Malloc failed: " << strerror(errno) << '\n';
      failed = true;
      break;
    }

    strncpy(argv[i], token.c_str(), token.length() + 1);

    ++i;
  }

  if (failed) {
    for (std::size_t j = 0; j < i; j++) {
      free(argv[j]);
      argv[j] = nullptr;
    }
    free(argv);
    argv = nullptr;

    return;
  }

  argc = tokens.size();
}

void freetokens(int argc, char **&argv) {
  for (std::size_t i = 0; i < argc; i++) {
    free(argv[i]);
    argv[i] = nullptr;
  }
  free(argv);
  argv = nullptr;
}

std::string getpath() {
  char *path = secure_getenv("PATH");
  if (!path) {
    return "";
  }

  return std::string(path);
}

std::string gethomedir() {
  char *homedir = secure_getenv("HOME");
  if (!homedir) {
    return "";
  }

  return std::string(homedir);
}

/* Used by filterdir */
static std::string filtercmd;

int filterdir(const struct dirent *pdir) {
  if (!pdir) {
    return 0;
  }

  return (strncmp(pdir->d_name, filtercmd.c_str(), 256) == 0);
}

std::string searchdir(const std::string &dir) {
  std::string cmd;
  struct dirent **namelist;
  int n;

  n = scandir(dir.c_str(), &namelist, filterdir, alphasort);
  /* TODO: handle error */

  if (n == 0) {
    return cmd;
  }

  while (n--) {
    if (strstr(namelist[n]->d_name, filtercmd.c_str()) != nullptr) {
      cmd = dir + "/" + std::string(namelist[n]->d_name);
    }
    free(namelist[n]);
  }
  free(namelist);

  return cmd;
}

void patherror(const std::string &func, int error, const std::string &arg) {
  switch (error) {
    case ENOENT:
    case ENOTDIR:
      std::cerr << arg << ": ";
      break;
    default:
      std::cerr << func << " failed: ";
  }
  std::cerr << strerror(errno) << '\n';
}

/* Classes */

/* Forward declaration */
struct shell_context;
class command;

class cmdprocessor {
  std::vector<std::shared_ptr<command>> commands;

  public:
  cmdprocessor();

  std::size_t add(std::shared_ptr<command> pcmd);

  std::shared_ptr<command> find(const char *name);

  int operator()(shell_context &ctx, int argc, char **argv);
};

struct shell_context {
  std::atomic<bool> exit_condition;
  cmdprocessor cmdproc;
  std::vector<std::string> path;
  std::string cwd;

  shell_context();

  void exit();

  std::shared_ptr<command> findcmd(const char *name);

  void loadpath(const std::string &pathstr);

  void clearpath();

  std::string searchpath(const std::string &cmd);

  std::string& setcwd(const std::string &newcwd = "");

  const std::string& getcwd() const;
};

class command {
  protected:
  std::string name;
  struct option *plongopts;

  public:
  command(const std::string &name_, struct option *plongopts_ = nullptr)
    : name(name_),
      plongopts(plongopts_) {
  }

  std::string& getname() {
    return name;
  }

  virtual int operator()(shell_context &ctx, int argc, char **argv) = 0;

  virtual ~command() = default;
};

cmdprocessor::cmdprocessor() : commands() {
}

std::size_t cmdprocessor::add(std::shared_ptr<command> pcmd) {
  commands.push_back(pcmd);
  return commands.size() - 1;
}

std::shared_ptr<command> cmdprocessor::find(const char *name) {
  auto it = std::find_if(
      commands.begin(),
      commands.end(),
      [name](std::shared_ptr<command> pcmd) {
        return pcmd->getname() == name;
      }
    );

  if (it == commands.end()) {
    return nullptr;
  }

  return *it;
}

int cmdprocessor::operator()(shell_context &ctx, int argc, char **argv) {
  int ret = 0;

  std::shared_ptr<command> pcmd = find(argv[0]);
  if (pcmd) {
    return (*pcmd)(ctx, argc, argv);
  }

  pid_t pid = fork();
  if (pid == -1) {
    /* handle error */
    return -1;
  }

  if (pid == 0) {
    /* child */
    execvp(argv[0], argv);

    ctx.exit();

    /* execvp failed */
    if (errno != ENOENT) {
      std::cerr << "execvp failed: " << strerror(errno) << '\n';
      return -1;
    }

  } else {
    /* parent */
    wait(&ret);

    return ret;
  }

  std::cout << argv[0] << ": command not found\n";
  return -1;
}


shell_context::shell_context() : exit_condition(false) {
  setcwd();
}

void shell_context::exit() {
  exit_condition = true;
}

std::shared_ptr<command> shell_context::findcmd(const char *name) {
  return cmdproc.find(name);
}

void shell_context::loadpath(const std::string &pathstr) {
  std::istringstream iss(pathstr);
  std::string dir;

  while (std::getline(iss, dir, ':')) {
    path.push_back(dir);
  }
}

void shell_context::clearpath() {
  path.clear();
}

std::string shell_context::searchpath(const std::string &cmd) {
  filtercmd = cmd;
  for (auto & pathdir : path) {
    std::string cmdpath = searchdir(pathdir);

    if (!cmdpath.empty()) {
      return cmdpath;
    }
  }

  return "";
}

std::string& shell_context::setcwd(const std::string &newcwd) {
  char *cwdstr = NULL;
  if (!newcwd.empty()) {
    cwdstr = realpath(newcwd.c_str(), NULL);

  } else {
    cwdstr = realpath(".", NULL);
  }
  if (!cwdstr) {
    patherror("realpath", errno, newcwd);

  } else {
    if (chdir(cwdstr) != 0) {
      patherror("chdir", errno, cwdstr);

    } else {
      cwd.clear();
      cwd.append(cwdstr);
    }

    free(cwdstr);
  }

  return cwd;
}

const std::string& shell_context::getcwd() const {
  return cwd;
}

/*
 * Exit code 0        Success
 * Exit code 1        General errors, Miscellaneous errors, such as "divide by zero" and other impermissible operations
 * Exit code 2        Misuse of shell builtins (according to Bash documentation)
 */
class cmd_exit : public command {
  public:
  cmd_exit() : command("exit", nullptr) {
  }

  int operator()(shell_context &ctx, int argc, char **argv) {
    int ret = 0;

    std::cout << "exit\n";

    for (std::size_t i = 1; i < argc; i++) {
      if (i > 1) {
        std::cout << "shell: " << name << ": too many arguments\n";
        return 1;
      }

      ret = std::atoi(argv[i]);
      for (std::size_t j = 0; j < strlen(argv[i]); j++) {
        if (!std::isdigit(argv[i][j])) {
          std::cout << "shell: " << name << ": " << argv[i]
            << ": numeric argument required\n";

          ret = 2;
          break;
        }
      }

      if (ret == 2) {
        break;
      }
    }

    ctx.exit();

    return ret;
  }
};

class cmd_echo : public command {
  public:
  cmd_echo() : command("echo", nullptr) {
  }

  int operator()(shell_context &ctx, int argc, char **argv) {
    for (std::size_t i = 1; i < argc; i++) {
      std::cout << argv[i];
      if (i < argc - 1) {
        std::cout << ' ';
      }
    }
    std::cout << '\n';

    return 0;
  }
};

class cmd_type : public command {
  public:
  cmd_type() : command("type", nullptr) {
  }

  int operator()(shell_context &ctx, int argc, char **argv) {
    if (argc == 1) {
      return 0;
    }

    for (std::size_t i = 1; i < argc; i++) {
      if (ctx.findcmd(argv[i]) != nullptr) {
        std::cout << argv[i] << " is a shell builtin\n";
        continue;
      }

      /* Else find executable in path */
      std::string cmdpath = ctx.searchpath(std::string(argv[i]));
      if (!cmdpath.empty()) {
        std::cout << argv[i] << " is " << cmdpath << '\n';
        continue;
      }

      std::cout << argv[i] << " not found\n";
    }

    return 1;
  }
};

class cmd_pwd : public command {
  public:
  cmd_pwd() : command("pwd", nullptr) {
  }

  int operator()(shell_context &ctx, int argc, char **argv) {
    std::cout << ctx.getcwd() << '\n';

    return 0;
  }
};

class cmd_cd : public command {
  public:
  cmd_cd() : command("cd", nullptr) {
  }

  int operator()(shell_context &ctx, int argc, char **argv) {
    std::string newcwd;

    if (argc == 1) {
      ctx.setcwd(gethomedir());

    } else if (argc > 1) {
      newcwd.append(argv[1]);

      /* Substitute '~' */
      std::string::size_type at = 0;
      while ((at = newcwd.find('~', at)) != std::string::npos) {
        newcwd.replace(at, 1, gethomedir());
      }

      char *cwdstr = realpath(newcwd.c_str(), NULL);
      if (!cwdstr) {
        patherror("realpath", errno, argv[1]);
        return -1;
      }

      ctx.setcwd(cwdstr);

      free(cwdstr);
    }

    return 0;
  }
};

int main(int argc, char **argv, char **envp) {
  int ret = 0;

  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  shell_context ctx;

  ctx.cmdproc.add(std::make_shared<cmd_exit>());
  ctx.cmdproc.add(std::make_shared<cmd_echo>());
  ctx.cmdproc.add(std::make_shared<cmd_type>());
  ctx.cmdproc.add(std::make_shared<cmd_pwd>());
  ctx.cmdproc.add(std::make_shared<cmd_cd>());

  /* Load path from env */
  std::string pathstr = getpath();
  ctx.loadpath(pathstr);

  while (!ctx.exit_condition) {
    std::cout << "$ ";
  
    std::string input;
    std::getline(std::cin, input);

    if (input.length() == 0) {
      continue;
    }

    int argc;
    char **argv;

    tokenize(input, argc, argv);

    ret = ctx.cmdproc(ctx, argc, argv);

    freetokens(argc, argv);
  }

  return ret;
}
