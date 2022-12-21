#include "argparser.hpp"

string to_string(vector<string> vec) {
  if (vec.empty())
    return "[]";
  stringstream s;
  s << "[\"" << vec[0] << "\"";
  for (auto i = 1; i < vec.size(); i++)
    s << ", \"" << vec[i] << "\"";
  s << "]";
  return s.str();
}

int main(int argc, char **argv) {
  auto parser = Parser::Builder()
                    .count(1)
                    .param(AliasedParameter::Builder()
                               .name("--param")
                               .alias("-O")
                               .args(ArgCount(1, 5))
                               .build())
                    .prefix("-")
                    .build();
  auto result = parser.parse(argc, argv);
  auto args = to_string(result.arguments);
  cout << "Аргументы: " << args << endl;
  for (auto parameter : result.parameters)
    cout << "Параметр \"" << parameter.first
         << "\": " << to_string(parameter.second) << endl;
}
