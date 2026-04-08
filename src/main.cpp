#include "CerberusCore.hpp"
#include <chrono>

int main() {
  Cerberus cerberus;

  while (!cerberus.has_an_error) {
    std::this_thread::sleep_for(std::chrono::hours(24));
  }

  return 0;
}
