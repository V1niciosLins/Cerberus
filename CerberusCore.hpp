#pragma once

#include <fstream>

inline static constexpr std::string_view CERBERUS_PATH{"./mocks/cerberus.conf"};

class CerberusCore {
  std::ofstream ofs{CERBERUS_PATH.data()};
  std::ifstream ifs{CERBERUS_PATH.data()};

public:
};
