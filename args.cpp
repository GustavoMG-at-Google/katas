#include <cassert>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>

enum FlagType { BOOL, INT32, STRING };
using FlagName = std::string;
using FlagSchema = std::unordered_map<FlagName, FlagType>;

template <typename T>
using FlagOfType = std::unordered_map<FlagName, T>;
struct Flags {
  Flags(const FlagSchema& schema) {
    for (const auto& [name, type] : schema) {
      if (type == BOOL) bool_flags[name] = false;
      if (type == INT32) int32_flags[name] = 0;
      if (type == STRING) string_flags[name] = "";
    }
  }
  FlagOfType<bool> bool_flags;
  FlagOfType<std::string> string_flags;
  FlagOfType<int32_t> int32_flags;
};

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

std::optional<bool> read_bool(TokenIterator*) { return true; }
std::optional<int32_t> read_int32(TokenIterator* it) {
  std::optional<std::string> token = it->next();
  if (!token.has_value()) {
    return std::nullopt;
  }
  try {
    return std::stoi(*token);
  } catch (...) {
    return std::nullopt;
  }
}
std::optional<std::string> read_string(TokenIterator* it) { return it->next(); }

struct ParseError {};
struct ReadName {};
struct ReadValue {
  FlagName flag_name;
};
struct Done {};
using State = std::variant<ParseError, ReadName, ReadValue, Done>;
class StateMachine {
 public:
  StateMachine(const FlagSchema& schema, TokenIterator* it, Flags* flags)
      : schema_(schema), it_(it), flags_(flags) {}
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
    auto schema_it = schema_.find(state.flag_name);
    if (schema_it == schema_.end()) {
      return ParseError{};
    }
    FlagType type = schema_it->second;
    if (type == BOOL) {
      std::optional<bool> value = read_bool(it_);
      if (!value.has_value()) {
        return ParseError{};
      }
      flags_->bool_flags[state.flag_name] = *value;
    } else if (type == INT32) {
      std::optional<int32_t> value = read_int32(it_);
      if (!value.has_value()) {
        return ParseError{};
      }
      flags_->int32_flags[state.flag_name] = *value;
    } else if (type == STRING) {
      std::optional<std::string> value = read_string(it_);
      if (!value.has_value()) {
        return ParseError{};
      }
      flags_->string_flags[state.flag_name] = *value;
    }
    return ReadName{};
  }
  State operator()(const ParseError& error) { return error; }
  State operator()(const Done& done) { return done; }

 private:
  const FlagSchema& schema_;
  TokenIterator* it_;
  Flags* flags_;
};

std::optional<Flags> parse_arg_list(const FlagSchema& schema,
                                    const std::string& arg_list) {
  TokenIterator it(arg_list);
  Flags flags(schema);
  StateMachine machine(schema, &it, &flags);
  State state = ReadName{};

  while (!std::holds_alternative<Done>(state) &&
         !std::holds_alternative<ParseError>(state)) {
    state = std::visit(machine, state);
  }
  if (std::holds_alternative<ParseError>(state)) {
    return std::nullopt;
  }
  return flags;
}

FlagSchema schema{{"l", BOOL}, {"p", INT32}, {"d", STRING}};

void test_happy() {
  std::string arg_list = "-l -p 1080 -d /hola/mundo";
  std::optional<Flags> flags = parse_arg_list(schema, arg_list);
  assert(flags->bool_flags["l"] == true);
  assert(flags->int32_flags["p"] == 1080);
  assert(flags->string_flags["d"] == "/hola/mundo");
}

void test_not_in_arg_list() {
  std::string arg_list = "";
  std::optional<Flags> flags = parse_arg_list(schema, arg_list);
  assert(flags->bool_flags["l"] == false);
  assert(flags->int32_flags["p"] == 0);
  assert(flags->string_flags["d"] == "");
}

void test_not_in_scheme() {
  std::string arg_list = "-x";
  std::optional<Flags> flags = parse_arg_list(schema, arg_list);
  assert(!flags.has_value());
}

void test_ambiguous_hyphen() {
  std::string arg_list = "-p -1080 -d -hola_mundo";
  std::optional<Flags> flags = parse_arg_list(schema, arg_list);
  assert(flags->int32_flags["p"] == -1080);
  assert(flags->string_flags["d"] == "-hola_mundo");
}

void test_early_exit() {
  std::string arg_list = "-d";
  std::optional<Flags> flags = parse_arg_list(schema, arg_list);
  assert(!flags.has_value());
}

void test_bad_int() {
  std::string arg_list = "-p abc";
  std::optional<Flags> flags = parse_arg_list(schema, arg_list);
  assert(!flags.has_value());
}

void test_duplicate() {
  std::string arg_list = "-p 1080 -p 88";
  std::optional<Flags> flags = parse_arg_list(schema, arg_list);
  assert(flags->int32_flags["p"] == 88);
}

int main() {
  test_happy();
  test_not_in_arg_list();
  test_not_in_scheme();
  test_ambiguous_hyphen();
  test_early_exit();
  test_bad_int();
  test_duplicate();

  std::cout << ":)" << std::endl;
}
