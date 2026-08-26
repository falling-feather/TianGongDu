#pragma once

#include <axmol.h>

class AppDelegate final : private ax::Application {
public:
  AppDelegate() = default;
  ~AppDelegate() override = default;

  void initGfxContextAttrs() override;
  bool applicationDidFinishLaunching() override;
  void applicationDidEnterBackground() override;
  void applicationWillEnterForeground() override;
  void applicationWillQuit() override;
};
