#include "AppDelegate.hpp"

#include <axmol.h>

#include <memory>

namespace {
std::unique_ptr<AppDelegate> appDelegate;
}

void axmol_wasm_app_exit() {
    appDelegate.reset();

#if AX_OBJECT_LEAK_DETECTION
    ax::Object::printLeaks();
#endif
}

int main() {
    appDelegate = std::make_unique<AppDelegate>();
    return ax::Application::getInstance()->run();
}
