#include <cassert>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>

class AbstractFlag {
 public:
  virtual ~AbstractFlag() = default;
  virtual bool setValue(std::stringstream&) = 0;
};

class BoolFlag : public AbstractFlag {
 public:
  bool getValue() { return value_; }
  bool setValue(std::stringstream&) override {
    value_ = true;
    return true;
  }

 private:
  bool value_ = false;
};

class Int32Flag : public AbstractFlag {
 public:
  int32_t getValue() { return value_; }
  bool setValue(std::stringstream& tokens) override {
    tokens >> value_;
    return !tokens.fail();
  }

 private:
  int32_t value_ = 0;
};

class StringFlag : public AbstractFlag {
 public:
  std::string getValue() { return value_; }
  bool setValue(std::stringstream& tokens) override {
    tokens >> value_;
    return !tokens.fail();
  }

 private:
  std::string value_ = "";
};

using FlagName = std::string;
using FlagRegistry = std::unordered_map<FlagName, AbstractFlag*>;

enum State { DONE, PARSE_ERROR, READ_FLAG };
State read_flag(const FlagRegistry& registry, std::stringstream& tokens) {
  std::string name_token;
  tokens >> name_token;
  if (tokens.fail()) {
    return DONE;
  }
  if (name_token.size() <= 1) {
    return PARSE_ERROR;
  }
  if (name_token.at(0) != '-') {
    return PARSE_ERROR;
  }
  std::string name = name_token.substr(1);
  auto registry_it = registry.find(name);
  if (registry_it == registry.end()) {
    return PARSE_ERROR;
  }
  AbstractFlag* flag = registry_it->second;
  bool success = flag->setValue(tokens);
  if (!success) {
    return PARSE_ERROR;
  }
  return READ_FLAG;
}

bool parse_arg_list(const FlagRegistry& registry, const std::string& arg_list) {
  std::stringstream tokens(arg_list);
  State state = READ_FLAG;

  while (state == READ_FLAG) {
    state = read_flag(registry, tokens);
  }
  return state == DONE;
}

void test_happy() {
  BoolFlag local;
  Int32Flag port;
  StringFlag directory;
  FlagRegistry registry;
  registry["l"] = &local;
  registry["p"] = &port;
  registry["d"] = &directory;
  std::string arg_list = "-l -p 1080 -d /hola/mundo";

  bool success = parse_arg_list(registry, arg_list);

  assert(success);
  assert(local.getValue() == true);
  assert(port.getValue() == 1080);
  assert(directory.getValue() == "/hola/mundo");
}

void test_not_in_arg_list() {
  BoolFlag local;
  Int32Flag port;
  StringFlag directory;
  FlagRegistry registry;
  registry["l"] = &local;
  registry["p"] = &port;
  registry["d"] = &directory;
  std::string arg_list = "";

  bool success = parse_arg_list(registry, arg_list);

  assert(success);
  assert(local.getValue() == false);
  assert(port.getValue() == 0);
  assert(directory.getValue() == "");
}

void test_not_in_scheme() {
  FlagRegistry registry;
  std::string arg_list = "-x";

  bool success = parse_arg_list(registry, arg_list);

  assert(!success);
}

void test_value_starts_with_hyphen() {
  Int32Flag port;
  StringFlag directory;
  FlagRegistry registry;
  registry["p"] = &port;
  registry["d"] = &directory;
  std::string arg_list = "-p -1080 -d -hola_mundo";

  bool success = parse_arg_list(registry, arg_list);

  assert(success);
  assert(port.getValue() == -1080);
  assert(directory.getValue() == "-hola_mundo");
}

void test_early_exit() {
  StringFlag directory;
  FlagRegistry registry;
  registry["d"] = &directory;
  std::string arg_list = "-d";

  bool success = parse_arg_list(registry, arg_list);

  assert(!success);
}

void test_bad_int() {
  Int32Flag port;
  FlagRegistry registry;
  registry["p"] = &port;
  std::string arg_list = "-p abc";

  bool success = parse_arg_list(registry, arg_list);

  assert(!success);
}

void test_duplicate() {
  Int32Flag port;
  FlagRegistry registry;
  registry["p"] = &port;
  std::string arg_list = "-p 1080 -p 88";

  bool success = parse_arg_list(registry, arg_list);

  assert(success);
  assert(port.getValue() == 88);
}

int main() {
  test_happy();
  test_not_in_arg_list();
  test_not_in_scheme();
  test_value_starts_with_hyphen();
  test_early_exit();
  test_bad_int();
  test_duplicate();

  std::cout << ":)" << std::endl;
}
