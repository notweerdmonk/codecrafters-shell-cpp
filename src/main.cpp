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
#include <errno.h>
#include <getopt.h>

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

/* Forward declaration */
struct shell_context;

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

class cmdprocessor {
  std::vector<std::shared_ptr<command>> commands;

  public:
  cmdprocessor() : commands() {
  }

  std::size_t add(std::shared_ptr<command> pcmd) {
    commands.push_back(pcmd);
    return commands.size() - 1;
  }

  int operator()(shell_context &ctx, int argc, char **argv) {
    auto it = std::find_if(
        commands.begin(),
        commands.end(),
        [argv](std::shared_ptr<command> pcmd) {
          return pcmd->getname() == argv[0];
        }
      );

    if (it == commands.end()) {
      std::cout << argv[0] << ": command not found\n";
      return -1;
    }

    return (*(*it))(ctx, argc, argv);
  }
};

struct shell_context {
  std::atomic<bool> exit_condition;
  cmdprocessor cmdproc;

  shell_context() : exit_condition(false) {
  }

  void exit() {
    exit_condition = true;
  }
};

void tokenize(const std::string &line, int &argc, char **&argv) {
  std::istringstream iss(line);
  std::string token;
  std::vector<std::string> tokens;

  while (std::getline(iss, token, ' ')) {
    lstrip<>(token, "\"");
    rstrip<>(token, "\"");
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

    strncpy(argv[i], token.c_str(), token.length());

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

int main() {
  int ret = 0;

  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  shell_context ctx;

  ctx.cmdproc.add(std::make_shared<cmd_exit>());

  while (!ctx.exit_condition) {
    std::cout << "$ ";
  
    std::string input;
    std::getline(std::cin, input);


    int argc;
    char **argv;

    tokenize(input, argc, argv);

    ret = ctx.cmdproc(ctx, argc, argv);
  }

  return ret;
}
