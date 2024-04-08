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

std::optional<FlagName> parse_flag_name(const std::string& token) {
  if (token.size() <= 1) {
    return std::nullopt;
  }
  if (token[0] != '-') {
    return std::nullopt;
  }
  return token.substr(1);
}

std::optional<std::string> parse_value_as_string(const std::string& token) {
  return token;
}

std::optional<int32_t> parse_value_as_int(const std::string& token) {
  try {
    return std::stoi(token);
  } catch (...) {
    return std::nullopt;
  }
}

struct FlagState {};
struct ValueState {
  FlagName flag_name;
  FlagType flag_type;
};
using State = std::variant<FlagState, ValueState>;
class StateMachine {
 public:
  StateMachine(const FlagSchema& schema, const std::string& token, Flags* flags)
      : schema_(schema), token_(token), flags_(flags) {}
  std::optional<State> operator()(const FlagState&) {
    std::optional<FlagName> flag_name = parse_flag_name(token_);
    if (!flag_name.has_value()) {
      return std::nullopt;
    }
    auto schema_it = schema_.find(*flag_name);
    if (schema_it == schema_.end()) {
      return std::nullopt;
    }
    FlagType type = schema_it->second;
    if (type == BOOL) {
      flags_->bool_flags[*flag_name] = true;
      return FlagState{};
    }
    return ValueState{*flag_name, type};
  }
  std::optional<State> operator()(const ValueState& state) {
    if (state.flag_type == INT32) {
      std::optional<int32_t> value = parse_value_as_int(token_);
      if (!value.has_value()) {
        return std::nullopt;
      }
      flags_->int32_flags[state.flag_name] = *value;
    } else {
      std::optional<std::string> value = parse_value_as_string(token_);
      if (!value.has_value()) {
        return std::nullopt;
      }
      flags_->string_flags[state.flag_name] = *value;
    }
    return FlagState{};
  }

 private:
  const FlagSchema& schema_;
  const std::string& token_;
  Flags* flags_;
};

std::optional<Flags> parse_arg_list(const FlagSchema& schema,
                                    const std::string& arg_list) {
  std::stringstream ss(arg_list);
  std::string token;
  Flags flags(schema);
  StateMachine machine(schema, token, &flags);
  State state = FlagState{};
  while (ss >> token) {
    std::optional<State> next_state = std::visit(machine, state);
    if (!next_state.has_value()) {
      return std::nullopt;
    }
    state = *next_state;
  }
  if (std::holds_alternative<ValueState>(state)) {
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
