#include <cassert>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>

class TokenIterator {
 public:
  TokenIterator(const std::string& arg_list) : token_stream(arg_list) {}
  std::optional<std::string> next() {
    std::string token;
    if (token_stream >> token) {
      return token;
    }
    return std::nullopt;
  }

 private:
  std::stringstream token_stream;
};

class AbstractFlag {
 public:
  virtual ~AbstractFlag() = default;
  virtual bool setValue(TokenIterator*) = 0;
};

class BoolFlag : public AbstractFlag {
 public:
  bool getValue() { return value_; }
  bool setValue(TokenIterator* it) override {
    value_ = true;
    return true;
  }

 private:
  bool value_ = false;
};

class Int32Flag : public AbstractFlag {
 public:
  int32_t getValue() { return value_; }
  bool setValue(TokenIterator* it) override {
    std::optional<std::string> token = it->next();
    if (!token.has_value()) {
      return false;
    }
    try {
      value_ = std::stoi(*token);
    } catch (...) {
      return false;
    }
    return true;
  }

 private:
  int32_t value_ = 0;
};

class StringFlag : public AbstractFlag {
 public:
  std::string getValue() { return value_; }
  bool setValue(TokenIterator* it) override {
    std::optional<std::string> token = it->next();
    if (!token.has_value()) {
      return false;
    }
    value_ = *token;
    return true;
  }

 private:
  std::string value_ = "";
};

using FlagName = std::string;
using FlagRegistry = std::unordered_map<FlagName, AbstractFlag*>;

struct ParseError {};
struct ReadName {};
struct ReadValue {
  FlagName flag_name;
};
struct Done {};
using State = std::variant<ParseError, ReadName, ReadValue, Done>;
class StateMachine {
 public:
  StateMachine(const FlagRegistry& registry, TokenIterator* it)
      : registry_(registry), it_(it) {}
  State operator()(const ReadName&) {
    std::optional<std::string> name = it_->next();
    if (!name.has_value()) {
      return Done{};
    }
    if (name->size() <= 1) {
      return ParseError{};
    }
    if (name->at(0) != '-') {
      return ParseError{};
    }
    return ReadValue{.flag_name = name->substr(1)};
  }
  State operator()(const ReadValue& state) {
    auto registry_it = registry_.find(state.flag_name);
    if (registry_it == registry_.end()) {
      return ParseError{};
    }
    AbstractFlag* flag = registry_it->second;
    bool success = flag->setValue(it_);
    if (!success) {
      return ParseError{};
    }
    return ReadName{};
  }
  State operator()(const ParseError& error) { return error; }
  State operator()(const Done& done) { return done; }

 private:
  const FlagRegistry& registry_;
  TokenIterator* it_;
};

bool parse_arg_list(const FlagRegistry& registry, const std::string& arg_list) {
  TokenIterator it(arg_list);
  StateMachine machine(registry, &it);
  State state = ReadName{};

  while (!std::holds_alternative<Done>(state) &&
         !std::holds_alternative<ParseError>(state)) {
    state = std::visit(machine, state);
  }
  if (std::holds_alternative<ParseError>(state)) {
    return false;
  }
  return true;
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
