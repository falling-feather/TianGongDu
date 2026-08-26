#include "AppDelegate.hpp"

#include <axmol.h>

#include <memory>

namespace {
std::unique_ptr<AppDelegate> app_delegate;
}

void axmol_wasm_app_exit() {
  app_delegate.reset();

#if AX_OBJECT_LEAK_DETECTION
  ax::Object::printLeaks();
#endif
}

int main() {
  app_delegate = std::make_unique<AppDelegate>();
  return ax::Application::getInstance()->run();
}
