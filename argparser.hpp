#pragma once

#include <algorithm>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

using namespace std;

void error(string message, int code = 1) {
  cerr << "Error: " << message << endl;
  exit(code);
}

void error(vector<string> messages, int code = 1) {
  for (auto message : messages)
    cerr << "Error: " << message << endl;
  exit(code);
}

template <typename A, typename B, typename C> struct Triad {
  A first;
  B second;
  C third;

  Triad(A first, B second, C third)
      : first(first), second(second), third(third) {}
};

class ArgCount {
  unsigned _min;
  unsigned _max;

public:
  ArgCount() : _min(0), _max(UINT32_MAX) {}

  ArgCount(unsigned number) : _min(number), _max(number) {}

  ArgCount(unsigned from, unsigned to) : _min(from), _max(to) {
    if (to < from)
      error("\"to\" is lower than \"from\"");
  }

  ArgCount(const ArgCount &other) {
    _min = other._min;
    _max = other._max;
  }

  static ArgCount from(unsigned from) { return ArgCount(from, UINT32_MAX); }

  static ArgCount to(unsigned to) { return ArgCount(0, to); }

  bool includes(unsigned number) const {
    if (number < _min)
      return false;
    if (_min == _max)
      return true;
    return number <= _max;
  }

  unsigned min() const { return _min; }

  unsigned max() const { return _max; }

  operator string() const {
    stringstream result;
    result << "ArgCount(from=";
    result << _min;
    result << ", to=";
    result << _max;
    result << ")";
    return result.str();
  }
};

struct Parameter {
  string name;
  ArgCount count;

  bool match(string str) { return name == str; }

  operator string() {
    stringstream result;
    result << "Paramater \"";
    result << name;
    result << "\" with ";
    result << (string)count;
    result << " arguments";
    return result.str();
  }
};

struct AliasedParameter : public Parameter {
  class Builder {
    string *_name;
    ArgCount *_count;
    vector<string> *_aliases;

  public:
    Builder() {
      _name = new string();
      _count = new ArgCount();
      _aliases = new vector<string>();
    }

    ~Builder() {
      delete _name;
      delete _count;
      delete _aliases;
    }

    Builder name(string name) {
      *_name = name;
      return *this;
    }

    Builder args(ArgCount count) {
      *_count = count;
      return *this;
    }

    Builder alias(string alias) {
      _aliases->push_back(alias);
      return *this;
    }

    AliasedParameter build() {
      return AliasedParameter(*_name, *_count, *_aliases);
    }
  };

  vector<string> aliases;

  AliasedParameter(string name, ArgCount count, vector<string> aliases)
      : aliases(aliases) {
    this->name = name;
    this->count = count;
  }

  bool match(string str) {
    if (Parameter::match(str))
      return true;
    return find_if(aliases.begin(), aliases.end(), [str](string alias) {
             return alias == str;
           }) != aliases.end();
  }

  operator string() {
    stringstream result;
    result << Parameter::operator string();
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

class Parser {
  string prefix;
  ArgCount count;
  vector<AliasedParameter> defined;

  Parameter find_parameter(string str) {
    for (auto param : defined)
      if (param.match(str))
        return param;
    error("undefined parameter: \"" + str + "\"");
    return Parameter();
  }

  bool is_parameter(string str) {
    return str.size() > prefix.size() && str.substr(0, prefix.size()) == prefix;
  }

public:
  class Builder {
    ArgCount *_count;
    vector<AliasedParameter> *_params;
    string *_prefix;

  public:
    Builder() {
      _count = new ArgCount();
      _params = new vector<AliasedParameter>();
      _prefix = new string();
    }

    ~Builder() {
      delete _count;
      delete _params;
      delete _prefix;
    }

    Builder count(ArgCount count) {
      *_count = count;
      return *this;
    }

    Builder param(AliasedParameter parameter) {
      _params->push_back(parameter);
      return *this;
    }

    Builder prefix(string prefix) {
      *_prefix = prefix;
      return *this;
    }

    Parser build() { return Parser(*_count, *_params, *_prefix); }
  };

  struct Result {
    vector<string> arguments;
    vector<pair<string, vector<string>>> parameters;
  };

  Parser(ArgCount count, vector<AliasedParameter> parameters,
         string prefix = "-")
      : count(count), defined(parameters), prefix(prefix) {}

  Result parse(vector<string> args) {
    //           prefix  parameter  arguments
    vector<Triad<string, Parameter, vector<string>>> stack;
    Result result;
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
      auto err = " arguments: expected " + (string)count + ", got " +
                 to_string(arg_size);
      if (count.min() > arg_size)
        error("not enough" + err);
      else
        error("too many" + err);
    }

    if (!stack.empty()) {
      vector<string> errors;
      for (auto item : stack) {
        auto arg_size = item.third.size();
        if (!count.includes(arg_size)) {
          auto name = "\"" + item.first + "\"";
          if (item.first != item.second.name)
            name += " (\"" + item.second.name + "\")";
          auto err = " arguments for parameter " + name + ": expected " +
                     (string)item.second.count + ", got " + to_string(arg_size);
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

  Result parse(int argc, char **argv) {
    vector<string> result;
    for (auto i = 1; i < argc; i++)
      result.push_back(argv[i]);
    return parse(result);
  }
};
