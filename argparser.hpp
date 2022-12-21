#pragma once

#include <algorithm>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

namespace argp {

void error(std::string message, int code = 1) {
  std::cerr << "Error: " << message << std::endl;
  exit(code);
}

void error(std::vector<std::string> messages, int code = 1) {
  for (auto message : messages)
    std::cerr << "Error: " << message << std::endl;
  exit(code);
}

template <typename A, typename B, typename C> struct triad {
  A first;
  B second;
  C third;

  triad(A first, B second, C third)
      : first(first), second(second), third(third) {}
};

class arg_count {
  unsigned _min;
  unsigned _max;

public:
  arg_count() : _min(0), _max(UINT32_MAX) {}

  arg_count(unsigned number) : _min(number), _max(number) {}

  arg_count(unsigned from, unsigned to) : _min(from), _max(to) {
    if (to < from)
      error("\"to\" is lower than \"from\"");
  }

  arg_count(const arg_count &other) {
    _min = other._min;
    _max = other._max;
  }

  static arg_count from(unsigned from) { return arg_count(from, UINT32_MAX); }

  static arg_count to(unsigned to) { return arg_count(0, to); }

  bool includes(unsigned number) const {
    if (number < _min)
      return false;
    if (_min == _max)
      return true;
    return number <= _max;
  }

  unsigned min() const { return _min; }

  unsigned max() const { return _max; }

  operator std::string() const {
    std::stringstream result;
    result << "ArgCount(from=";
    result << _min;
    result << ", to=";
    result << _max;
    result << ")";
    return result.str();
  }
};

struct parameter {
  std::string name;
  arg_count count;

  bool match(std::string str) { return name == str; }

  operator std::string() {
    std::stringstream result;
    result << "Paramater \"";
    result << name;
    result << "\" with ";
    result << (std::string)count;
    result << " arguments";
    return result.str();
  }
};

struct aliased_parameter : public parameter {
  class builder {
    std::string *_name;
    arg_count *_count;
    std::vector<std::string> *_aliases;

  public:
    builder() {
      _name = new std::string();
      _count = new arg_count();
      _aliases = new std::vector<std::string>();
    }

    ~builder() {
      delete _name;
      delete _count;
      delete _aliases;
    }

    builder name(std::string name) {
      *_name = name;
      return *this;
    }

    builder args(arg_count count) {
      *_count = count;
      return *this;
    }

    builder alias(std::string alias) {
      _aliases->push_back(alias);
      return *this;
    }

    aliased_parameter build() {
      return aliased_parameter(*_name, *_count, *_aliases);
    }
  };

  std::vector<std::string> aliases;

  aliased_parameter(std::string name, arg_count count, std::vector<std::string> aliases)
      : aliases(aliases) {
    this->name = name;
    this->count = count;
  }

  bool match(std::string str) {
    if (parameter::match(str))
      return true;
    return find_if(aliases.begin(), aliases.end(), [str](std::string alias) {
             return alias == str;
           }) != aliases.end();
  }

  operator std::string() {
    std::stringstream result;
    result << parameter::operator std::string();
    if (!aliases.empty()) {
      result << ", aliases: \"";
      result << aliases[0];
      result << "\"";
      if (aliases.size() > 1) {
        for (auto i = 1; i < aliases.size(); i++) {
          result << ", \"";
          result << aliases[i];
          result << "\"";
        }
      }
    }
    return result.str();
  }
};

class parser {
  std::string prefix;
  arg_count count;
  std::vector<aliased_parameter> defined;

  parameter find_parameter(std::string str) {
    for (auto param : defined)
      if (param.match(str))
        return param;
    error("undefined parameter: \"" + str + "\"");
    return parameter();
  }

  bool is_parameter(std::string str) {
    return str.size() > prefix.size() && str.substr(0, prefix.size()) == prefix;
  }

public:
  class builder {
    arg_count *_count;
    std::vector<aliased_parameter> *_params;
    std::string *_prefix;

  public:
    builder() {
      _count = new arg_count();
      _params = new std::vector<aliased_parameter>();
      _prefix = new std::string();
    }

    ~builder() {
      delete _count;
      delete _params;
      delete _prefix;
    }

    builder count(arg_count count) {
      *_count = count;
      return *this;
    }

    builder param(aliased_parameter parameter) {
      _params->push_back(parameter);
      return *this;
    }

    builder prefix(std::string prefix) {
      *_prefix = prefix;
      return *this;
    }

    parser build() { return parser(*_count, *_params, *_prefix); }
  };

  struct result {
    std::vector<std::string> arguments;
    std::vector<std::pair<std::string, std::vector<std::string>>> parameters;
  };

  parser(arg_count count, std::vector<aliased_parameter> parameters,
         std::string prefix = "-")
      : count(count), defined(parameters), prefix(prefix) {}

  result parse(std::vector<std::string> args) {
    //                prefix       parameter  arguments
    std::vector<triad<std::string, parameter, std::vector<std::string>>> stack;
    result result;
    for (auto ptr = args.begin(); ptr != args.end(); ptr++) {
      if (is_parameter(*ptr)) {
        stack.push_back({*ptr, find_parameter(*ptr), {}});
      } else if (stack.empty()) {
        result.arguments.push_back(*ptr);
      } else {
        auto &back = stack.back();
        if (back.second.count.max() == back.third.size()) {
          result.parameters.push_back({back.second.name, back.third});
          stack.pop_back();
        } else {
          back.third.push_back(*ptr);
        }
      }
    }

    auto arg_size = result.arguments.size();
    if (!count.includes(arg_size)) {
      auto err = " arguments: expected " + (std::string)count + ", got " +
                 std::to_string(arg_size);
      if (count.min() > arg_size)
        error("not enough" + err);
      else
        error("too many" + err);
    }

    if (!stack.empty()) {
      std::vector<std::string> errors;
      for (auto item : stack) {
        auto arg_size = item.third.size();
        if (!count.includes(arg_size)) {
          auto name = "\"" + item.first + "\"";
          if (item.first != item.second.name)
            name += " (\"" + item.second.name + "\")";
          auto err = " arguments for parameter " + name + ": expected " +
                     (std::string)item.second.count + ", got " + std::to_string(arg_size);
          if (count.min() > arg_size)
            error("not enough" + err);
          else
            error("too many" + err);
        } else {
          result.parameters.push_back({item.first, item.third});
        }
      }
    }
    return result;
  }

  result parse(int argc, char **argv) {
    std::vector<std::string> result;
    for (auto i = 1; i < argc; i++)
      result.push_back(argv[i]);
    return parse(result);
  }
};

parser::builder P() { return parser::builder(); }

aliased_parameter::builder R() { return aliased_parameter::builder(); }

} // namespace argparser