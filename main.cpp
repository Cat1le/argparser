#include "argparser.hpp"

std::string to_string(std::vector<std::string> vec) {
  if (vec.empty())
    return "[]";
  std::stringstream s;
  s << "[\"" << vec[0] << "\"";
  for (auto i = 1; i < vec.size(); i++)
    s << ", \"" << vec[i] << "\"";
  s << "]";
  return s.str();
}

int main(int argc, char **argv) {
  auto p = argp::P()
                    .count(1)
                    .param(argp::R()
                               .name("--param")
                               .alias("-O")
                               .args(argp::arg_count(1, 5))
                               .build())
                    .prefix("-")
                    .build();
  auto result = p.parse(argc, argv);
  auto args = to_string(result.arguments);
  std::cout << "Аргументы: " << args << std::endl;
  for (auto parameter : result.parameters)
    std::cout << "Параметр \"" << parameter.first
         << "\": " << to_string(parameter.second) << std::endl;
}
