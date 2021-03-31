#ifndef SIM_HELPER_HPP
#define SIM_HELPER_HPP

#include <string>

class SimHelper {
public:
  static std::string
  getEnvVar(std::string const& key)
  {
    char const* val = getenv(key.c_str());
    return val == NULL ? std::string() : std::string(val);
  }

  static std::string
  getEnvVariableStr(std::string name, std::string defaultValue)
  {
    std::string tmp = getEnvVar(name);
    if (!tmp.empty()) {
      return tmp;
    }
    else {
      return defaultValue;
    }
  }

  static int
  getEnvVariable(std::string name, int defaultValue)
  {
    std::string tmp = getEnvVar(name);
    if (!tmp.empty()) {
      return std::stoi(tmp);
    }
    else {
      return defaultValue;
    }
  }

  static double
  getEnvVariable(std::string name, double defaultValue)
  {
    std::string tmp = getEnvVar(name);
    if (!tmp.empty()) {
      return std::stod(tmp);
    }
    else {
      return defaultValue;
    }
  }

  static bool
  getEnvVariable(std::string name, bool defaultValue)
  {
    std::string tmp = getEnvVar(name);
    if (!tmp.empty()) {
      return (tmp == "TRUE" || tmp == "true");
    }
    else {
      return defaultValue;
    }
  }
};

#endif // SIM_HELPER_HPP