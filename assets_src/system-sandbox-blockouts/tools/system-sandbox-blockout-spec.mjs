export const TASK_ID = "ART-002";
export const AUTHORING_BASE = "99a4093c51b99bc8067e10364f591011efe328ab";
export const AUTHORITATIVE_ROOT = "27fb87c8af74b48e0bfb8f4ef6da2f8e96d6560e";
export const LINEAGE = Object.freeze({
  assetCommit: "f95454c9fa51bdaf593dc354ac21a6ed770ee31c",
  bindingTree: "e02ff077c0d1d57923064a2273fc9a049ec475eb",
  ledgerCommit: AUTHORING_BASE
});

export const PACKAGE_ID = "system_demo.presentation.blockouts";
export const STATUS = "Blockout";
export const INTEGRATION_STATUS = "not-integrated";
export const LICENSE_STATUS = "review-recorded-not-release-cleared";
export const STABLE_ID_PATTERN = /^[a-z][a-z0-9]*(?:[._-][a-z0-9]+)*$/;

export const VIEWPORTS = Object.freeze([
  Object.freeze({ id: "1280x720", width: 1280, height: 720 }),
  Object.freeze({ id: "1920x1080", width: 1920, height: 1080 }),
  Object.freeze({ id: "1230x692", width: 1230, height: 692 })
]);

const platforms = () => ({
  web: {
    variantState: "not-built",
    integrationState: INTEGRATION_STATUS,
    carrierPlan: "small opaque or alpha-clip presentation artifact behind a future resolver"
  },
  windows: {
    variantState: "not-built",
    integrationState: INTEGRATION_STATUS,
    carrierPlan: "small opaque or alpha-clip presentation artifact behind a future resolver"
  }
});

const anchor = (name, x, y, height, role) => ({
  name,
  localMm: { x, y, height },
  role
});

const asset = (value) => Object.freeze({
  owner: "ART",
  package: PACKAGE_ID,
  status: STATUS,
  integrationStatus: INTEGRATION_STATUS,
  axes: Object.freeze({ right: "+x", forward: "+y", up: "+height" }),
  unit: "millimeter",
  facingConvention: "visual forward is +y; authored facing is consumed from the external sandbox definition",
  platformPlan: platforms(),
  ...value
});

export const ASSETS = Object.freeze([
  asset({
    slug: "player",
    stableAssetId: "asset.system_demo.player",
    sandboxAssetKind: "player",
    displayName: "系统 Demo 玩家灰盒",
    sourceMode: "project-grammar-reuse",
    reuseLineage: "project-owned upright lantern-bearer grammar; simplified anew for this package",
    fallbackId: "fallback.system_demo.player",
    shapeToken: "upright-lantern-diagonal",
    states: ["neutral", "eavesguard", "flower-turn"],
    visualBoundsMm: { width: 1115, depth: 650, height: 1800 },
    anchors: [
      anchor("foot_center", 0, 0, 0, "canonical visual foot point"),
      anchor("weapon_contact_visual", 420, 220, 980, "presentation-only contact marker"),
      anchor("hurt_center_visual", 0, 0, 980, "presentation-only feedback center")
    ],
    nonColorChannels: ["upright narrow trapezoid", "lamp-pin dot", "long folded-umbrella diagonal"],
    dependencies: ["external player pose", "external stance snapshot", "future presentation resolver"],
    budget: { targetTransportKiB: 850, targetGpuTextureMiB: 4.5, targetDrawCalls: 1, peakTransparentCoveragePercent: 5 }
  }),
  asset({
    slug: "enemy-pressure",
    stableAssetId: "asset.system_demo.enemy.pressure",
    sandboxAssetKind: "actor",
    displayName: "系统 Demo 压力职责敌人灰盒",
    sourceMode: "project-grammar-reuse",
    reuseLineage: "project-owned broken-canopy pressure grammar; simplified anew for this package",
    fallbackId: "fallback.system_demo.enemy.pressure",
    shapeToken: "broken-triangle-radial-ribs",
    states: ["neutral", "anticipation", "recovery"],
    visualBoundsMm: { width: 1480, depth: 900, height: 1620 },
    anchors: [
      anchor("rib_hub_ground_center", 0, 0, 0, "canonical visual foot point"),
      anchor("forward_contact_visual", 0, 620, 620, "presentation-only forward contact marker"),
      anchor("hurt_center_visual", 0, 0, 760, "presentation-only feedback center")
    ],
    nonColorChannels: ["low broken triangle", "radial rib feet", "inward canopy compression"],
    dependencies: ["external actor pose", "external hostile role assignment", "future presentation resolver"],
    budget: { targetTransportKiB: 620, targetGpuTextureMiB: 3.2, targetDrawCalls: 1, peakTransparentCoveragePercent: 4 }
  }),
  asset({
    slug: "enemy-flanker",
    stableAssetId: "asset.system_demo.enemy.flanker",
    sandboxAssetKind: "actor",
    displayName: "系统 Demo 侧袭职责敌人灰盒",
    sourceMode: "new-source",
    fallbackId: "fallback.system_demo.enemy.flanker",
    shapeToken: "asymmetric-high-zigzag-shadow",
    states: ["neutral", "side-windup", "recovery"],
    visualBoundsMm: { width: 1880, depth: 700, height: 1940, bodyHeight: 1240, bodyRootHeight: 700 },
    anchors: [
      anchor("projected_foot_center", 0, 0, 0, "visual ground projection; never a gameplay origin"),
      anchor("body_root_h700", 0, 0, 700, "high visual body root"),
      anchor("side_contact_visual", 680, 420, 940, "presentation-only side contact marker"),
      anchor("hurt_center_visual", 0, 0, 1180, "presentation-only feedback center")
    ],
    nonColorChannels: ["high wide zigzag", "single long asymmetric wing", "ground projection tightens before lateral motion"],
    dependencies: ["external actor pose", "external hostile role assignment", "future presentation resolver"],
    budget: { targetTransportKiB: 620, targetGpuTextureMiB: 4, targetDrawCalls: 2, peakTransparentCoveragePercent: 4 }
  }),
  asset({
    slug: "enemy-elite",
    stableAssetId: "asset.system_demo.enemy.elite",
    sandboxAssetKind: "actor",
    displayName: "系统 Demo 精英敌人灰盒",
    sourceMode: "new-source",
    fallbackId: "fallback.system_demo.enemy.elite",
    shapeToken: "split-canopy-counterweight-triad",
    states: ["neutral", "heavy-windup", "staggered"],
    visualBoundsMm: { width: 2100, depth: 1100, height: 2520 },
    anchors: [
      anchor("foot_center", 0, 0, 0, "canonical visual foot point"),
      anchor("counterweight_core", 0, 0, 1320, "central presentation core"),
      anchor("left_contact_visual", -720, 280, 1220, "presentation-only left contact marker"),
      anchor("right_contact_visual", 720, 280, 1220, "presentation-only right contact marker"),
      anchor("ground_warning_visual", 0, 520, 0, "presentation-only ground warning origin")
    ],
    nonColorChannels: ["split canopy crown", "vertical counterweight column", "three hanging weights and slower cadence"],
    dependencies: ["external actor pose", "external elite assignment", "future presentation resolver"],
    budget: { targetTransportKiB: 900, targetGpuTextureMiB: 6, targetDrawCalls: 3, peakTransparentCoveragePercent: 5 }
  }),
  asset({
    slug: "obstacle-tension-gate",
    stableAssetId: "asset.system_demo.obstacle.tension_gate",
    sandboxAssetKind: "obstacle",
    displayName: "系统 Demo 独立绷架障碍灰盒",
    sourceMode: "new-source",
    fallbackId: "fallback.system_demo.obstacle.tension_gate",
    shapeToken: "solid-threshold-cross-brace",
    states: ["static"],
    visualBoundsMm: { width: 600, depth: 1000, height: 2500 },
    anchors: [anchor("base_center", 0, 0, 0, "visual base center only")],
    nonColorChannels: ["solid threshold rectangle", "cross-braced tension frame", "no pulse or operation glyph"],
    dependencies: ["external ground blocker bounds", "future presentation resolver"],
    budget: { targetTransportKiB: 180, targetGpuTextureMiB: 1.5, targetDrawCalls: 2, peakTransparentCoveragePercent: 0 }
  }),
  asset({
    slug: "interaction-console",
    stableAssetId: "asset.system_demo.interaction.console",
    sandboxAssetKind: "interaction",
    displayName: "系统 Demo 独立互动台灰盒",
    sourceMode: "project-grammar-reuse",
    reuseLineage: "project-owned clipped inspection and operation grammar; simplified anew for this package",
    fallbackId: "fallback.system_demo.interaction.console",
    shapeToken: "clipped-console-hand-slot",
    states: ["available", "focused", "consumed-visual"],
    visualBoundsMm: { width: 900, depth: 650, height: 1250 },
    anchors: [
      anchor("base_center", 0, 0, 0, "visual base center"),
      anchor("select_visual", 0, 120, 820, "presentation-only selection focus"),
      anchor("operate_visual", 180, 180, 720, "presentation-only operation marker")
    ],
    nonColorChannels: ["clipped-corner console", "recessed hand slot", "short lever changes position"],
    dependencies: ["external interaction pose", "external interaction state", "future presentation resolver"],
    budget: { targetTransportKiB: 140, targetGpuTextureMiB: 1, targetDrawCalls: 2, peakTransparentCoveragePercent: 1 }
  }),
  asset({
    slug: "mechanism-gate",
    stableAssetId: "asset.system_demo.mechanism.gate",
    sandboxAssetKind: "mechanism",
    displayName: "系统 Demo 独立闸门机关灰盒",
    sourceMode: "project-grammar-reuse",
    reuseLineage: "project-owned counterweight and tension-link grammar; simplified anew for this package",
    fallbackId: "fallback.system_demo.mechanism.gate",
    shapeToken: "offset-gate-link-counterweight",
    states: ["idle", "moving-visual", "settled-visual"],
    visualBoundsMm: { width: 1200, depth: 800, height: 2300 },
    anchors: [
      anchor("base_center", 0, 0, 0, "visual base center"),
      anchor("link_visual", -280, 0, 1180, "presentation-only linkage marker"),
      anchor("weight_visual", 360, 0, 620, "presentation-only counterweight marker")
    ],
    nonColorChannels: ["offset gate post", "visible diagonal linkage", "counterweight changes height"],
    dependencies: ["external mechanism pose", "external mechanism state", "future presentation resolver"],
    budget: { targetTransportKiB: 160, targetGpuTextureMiB: 1.25, targetDrawCalls: 2, peakTransparentCoveragePercent: 1 }
  }),
  asset({
    slug: "safe-point-lamp-shelter",
    stableAssetId: "asset.system_demo.safe_point.lamp_shelter",
    sandboxAssetKind: "safe_point",
    displayName: "系统 Demo 独立灯棚安全点灰盒",
    sourceMode: "new-source",
    fallbackId: "fallback.system_demo.safe_point.lamp_shelter",
    shapeToken: "closed-double-ring-tripod-lamp",
    states: ["available", "current-visual"],
    visualBoundsMm: { width: 1400, depth: 1400, height: 1900, groundRingDiameter: 1200 },
    anchors: [
      anchor("base_center", 0, 0, 0, "visual base center"),
      anchor("lamp_core", 0, 0, 1450, "presentation-only lamp marker"),
      anchor("return_facing_visual", 0, 520, 0, "visual facing hint; not a spawn writer")
    ],
    nonColorChannels: ["closed double ground ring", "three-rib shelter", "single upward lamp core"],
    dependencies: ["external safe point pose", "external current-safe-point snapshot", "future presentation resolver"],
    budget: { targetTransportKiB: 180, targetGpuTextureMiB: 1.5, targetDrawCalls: 2, peakTransparentCoveragePercent: 2 }
  }),
  asset({
    slug: "skill-eavesguard-telegraph",
    stableAssetId: "asset.system_demo.skill.eavesguard.telegraph",
    sandboxAssetKind: "effect",
    displayName: "檐守技能攻击提示灰盒",
    sourceMode: "new-source",
    fallbackId: "fallback.system_demo.skill.eavesguard.telegraph",
    shapeToken: "solid-wedge-inward-ribs",
    states: ["early-visual", "late-visual", "boundary-visual"],
    nominalPresentationExtentMm: { forward: 1200, lateral: 900, height: 0 },
    anchors: [
      anchor("source_foot_visual", 0, 0, 0, "presentation source point"),
      anchor("forward_axis_visual", 0, 600, 0, "presentation direction marker")
    ],
    nonColorChannels: ["solid brace wedge", "three inward ribs", "edge compression increases over time"],
    dependencies: ["external ability geometry", "external ability phase", "future presentation resolver"],
    budget: { targetTransportKiB: 220, targetGpuTextureMiB: 2.665, targetDrawCalls: 2, peakTransparentCoveragePercent: 8, peakLayerCount: 2 }
  }),
  asset({
    slug: "skill-eavesguard-hit",
    stableAssetId: "asset.system_demo.skill.eavesguard.hit",
    sandboxAssetKind: "effect",
    displayName: "檐守技能命中反馈灰盒",
    sourceMode: "new-source",
    fallbackId: "fallback.system_demo.skill.eavesguard.hit",
    shapeToken: "stacked-compression-bars",
    states: ["contact-visual", "decay-visual"],
    nominalPresentationExtentMm: { width: 800, depth: 500, height: 1100 },
    anchors: [
      anchor("contact_visual", 0, 0, 520, "external contact projection"),
      anchor("target_center_visual", 0, 0, 720, "presentation-only target center")
    ],
    nonColorChannels: ["stacked compression bars", "single forward rebound", "rectangular impact notch"],
    dependencies: ["external contact result", "external target pose", "future presentation resolver"],
    budget: { targetTransportKiB: 200, targetGpuTextureMiB: 2.665, targetDrawCalls: 2, peakTransparentCoveragePercent: 6, peakLayerCount: 2 }
  }),
  asset({
    slug: "skill-flower-turn-telegraph",
    stableAssetId: "asset.system_demo.skill.flower_turn.telegraph",
    sandboxAssetKind: "effect",
    displayName: "翻花技能攻击提示灰盒",
    sourceMode: "new-source",
    fallbackId: "fallback.system_demo.skill.flower_turn.telegraph",
    shapeToken: "broken-double-arc-diagonal",
    states: ["early-visual", "late-visual", "boundary-visual"],
    nominalPresentationExtentMm: { sweep: 1800, lateral: 1800, height: 0 },
    anchors: [
      anchor("source_foot_visual", 0, 0, 0, "presentation source point"),
      anchor("sweep_axis_visual", 720, 320, 0, "presentation sweep marker")
    ],
    nonColorChannels: ["broken double arc", "diagonal crossing gap", "lateral sweep motion"],
    dependencies: ["external ability geometry", "external ability phase", "future presentation resolver"],
    budget: { targetTransportKiB: 220, targetGpuTextureMiB: 2.665, targetDrawCalls: 2, peakTransparentCoveragePercent: 8, peakLayerCount: 2 }
  }),
  asset({
    slug: "skill-flower-turn-hit",
    stableAssetId: "asset.system_demo.skill.flower_turn.hit",
    sandboxAssetKind: "effect",
    displayName: "翻花技能命中反馈灰盒",
    sourceMode: "new-source",
    fallbackId: "fallback.system_demo.skill.flower_turn.hit",
    shapeToken: "crossed-shear-cuts",
    states: ["contact-visual", "decay-visual"],
    nominalPresentationExtentMm: { width: 1200, depth: 700, height: 1200 },
    anchors: [
      anchor("contact_visual", 0, 0, 560, "external contact projection"),
      anchor("target_center_visual", 0, 0, 760, "presentation-only target center")
    ],
    nonColorChannels: ["crossed diagonal cuts", "opposed lateral shear", "open center diamond"],
    dependencies: ["external contact result", "external target pose", "future presentation resolver"],
    budget: { targetTransportKiB: 200, targetGpuTextureMiB: 2.665, targetDrawCalls: 2, peakTransparentCoveragePercent: 6, peakLayerCount: 2 }
  })
]);

export const EXPECTED_STABLE_ASSET_KINDS = Object.freeze(new Map(
  ASSETS.map((entry) => [entry.stableAssetId, entry.sandboxAssetKind])
));

export const NEW_SOURCE_ASSETS = Object.freeze(
  ASSETS.filter((entry) => entry.sourceMode === "new-source")
);

export function cloneAssetSpecs() {
  return structuredClone(ASSETS);
}
