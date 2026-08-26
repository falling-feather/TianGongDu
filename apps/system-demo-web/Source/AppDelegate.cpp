#include "AppDelegate.hpp"

#include "SystemDemoLayer.hpp"

namespace {

constexpr float design_width = 1280.0F;
constexpr float design_height = 720.0F;

} // namespace

void AppDelegate::initGfxContextAttrs() {
  GfxContextAttrs context_attributes{8, 8, 8, 8, 24, 8, 0};
  ax::RenderView::setGfxContextAttrs(context_attributes);
}

bool AppDelegate::applicationDidFinishLaunching() {
  auto *director = ax::Director::getInstance();
  auto *render_view = director->getRenderView();
  if (render_view == nullptr) {
    render_view = ax::RenderViewImpl::createWithRect(
        "TianGongDu System Demo 0.8.4",
        ax::Rect(0.0F, 0.0F, design_width, design_height));
    director->setRenderView(render_view);
  }

#ifndef NDEBUG
  director->setStatsDisplay(true);
#else
  director->setStatsDisplay(false);
#endif
  director->setAnimationInterval(1.0F / 60.0F);
  render_view->setDesignResolutionSize(design_width, design_height,
                                       ResolutionPolicy::SHOW_ALL);

  auto *scene = createSystemDemoScene();
  if (scene == nullptr) {
    return false;
  }
  director->runWithScene(scene);
  return true;
}

void AppDelegate::applicationDidEnterBackground() {
  ax::Director::getInstance()->stopAnimation();
}

void AppDelegate::applicationWillEnterForeground() {
  ax::Director::getInstance()->startAnimation();
}

void AppDelegate::applicationWillQuit() {}
