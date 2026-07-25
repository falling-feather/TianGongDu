# Sandbox Content Workbench

这是轻量 Sandbox 作者编辑器：在一个启动时固定的 workspace 中打开既有 `tgd.sandbox.authoring` `1.1.0` JSON，浏览并编辑 player、actors、groundBlockers、safePoints、interactions、mechanisms，然后以 CAS 保护保存或显式重载。Workbench 还可以通过 `TOOLS-002A` 的受信任加载器调用唯一 DEV compiler/provider，并把共享内容诊断呈现给作者。

## 边界

- Workbench 只拥有作者草稿、浏览器表单、revision/dirty/last-valid 协调和本地 workspace I/O。
- Stable ID 只读；本切片不新增、删除或改名对象，也不提供通用 JSON/target 编辑器。
- JavaScript author shape 只是非权威结构门。页面中的“结构草稿”不表示通过 DEV validator、可导出或可玩。
- DEV 提供唯一 compiler/provider bridge。controller 只提交自身 `lastValidDocument` 的 runtime projection；JavaScript 不复制 Stable ID、引用、图、binding、容量或 Gameplay validator。
- `lastValidDocument`、server-private `validatedOwningPackage` 与未来 `runningPreviewSession` 是三层独立真相。只有 provider 返回 published 完整结果才替换已准备包；任何 validation、stale、transport、decode 或 provider 失败都保留上一份包与作者草稿。
- 浏览器 JSON 状态只接收有界表现诊断与 opaque lease，不接收 provider generation/checksum、canonical package bytes、Stable key、CAS、模块路径或异常堆栈。只有用户显式导出时，独立二进制响应才逐字节交付当前 fresh、server-private 的 canonical package。诊断定位只是 section/record/Stable ID/field 到现有六类表单的表现映射。
- 当前 Export 只把既有 canonical `.tgdsbx` 交给浏览器下载，不重编译、不重编码、不写 workspace，也不表示操作系统已经持久保存。仍没有 GAME Session、Windows/Web Preview、F1/Profile/奖励或任意命令执行。页面中的“包已准备；尚未导出，也未启动 Preview 或试玩。”只说明本地 provider 已接受一个完整候选。
- 页面永久使用“字段格式可保存；尚未进行内容与玩法校验。”，不会向用户暴露 CAS、hash、JSONPath 或技术异常。
- 六类对象共用一个键盘 Tab-stop tree；对象字段缓冲按对象保留，Escape 撤销当前字段，存在任意未应用缓冲时 Save 禁用。
- 外部修改不会抢焦；处理对话框由用户主动打开，首焦为“继续编辑”，取消或 Escape 返回触发控件。

## 运行

需要 Node.js 20 或更高版本。服务器固定绑定 `127.0.0.1` 的随机端口；workspace root 可以是绝对路径，但浏览器中的文档路径必须是该 root 内的既有相对 `.json` 文件。

```powershell
npm --prefix apps/content-workbench start -- --workspace "D:\path\to\sandbox-workspace" --sandbox-service-build "build\web-service"
```

`--sandbox-service-build` 是仓库内 CMake binary directory 的相对路径。省略或加载失败时编辑器仍可打开和保存作者草稿，但“共享内容检查”显示 unavailable。服务器会打印一次本地 URL；它只公开三项静态资源和有限的 open/reload/update/save/content-check/state API，不启动浏览器，也不执行外部命令。

## Sandbox 编译模块加载器

`src/sandbox-package-service-loader.mjs` 只接受仓库根内的 CMake binary directory 相对路径，并只解析该目录下固定的两个 generated artifact：

```text
dist/web/tgd-sandbox-package-service-abi.mjs
dist/web/tgd-sandbox-package-service-abi.wasm
```

加载器拒绝绝对路径、URL、`.`、`..`、空段、符号链接 artifact、仓库外 realpath、未知 module factory、ABI 不匹配、初始化失败和同一 artifact 的重复加载。它不接受浏览器或 authoring JSON 提供 module path，不使用 `eval`，也不执行外部命令。每个成功加载的 service 只复用 `sandbox-package-service-client.mjs`，输出 owning JavaScript result/diagnostics 值；classification、fixture、label 和 editor namespace 不参与 compiler 语义。

`npm start` 只有收到 server-side `--sandbox-service-build` 时才尝试加载固定产物。模块由服务器持有到关闭，浏览器不能提供或替换路径。共享检查与 Export 分别 single-flight 且互斥；Apply、Save、Open、Reload 或更新检查会使旧 HTTP 响应过期，过期响应不替换当前文档、诊断、焦点、已准备包或创建下载。真实 generated-WASM 门可用仓库相对 CMake binary directory 指定：

```powershell
$env:TGD_SANDBOX_SERVICE_BUILD_DIRECTORY = "build\web-service"
$env:TGD_SANDBOX_SERVICE_WASM_MODULE = (Resolve-Path "build\web-service\dist\web\tgd-sandbox-package-service-abi.mjs")
npm --prefix apps/content-workbench test
```

`TGD_SANDBOX_SERVICE_WASM_MODULE` 仅供既有 DEV generated-module probe 使用；生产 loader 本身不读取该环境变量或任意 module 文件路径。`TGD_SANDBOX_SERVICE_BUILD_DIRECTORY` 仅供自动化测试把同一仓库相对 binary directory 传给 loader/browser gate。检查成功仍不是下载或 Preview 证据。

## Canonical package Export

`POST /api/package-export` 只接受当前 document lease、revision 与 fresh prepared-package lease。controller 从 server-private `validatedOwningPackage.packageBytes` 复制一个导出快照并复核既有 package SHA-256；它不读取 DOM、不提交 authoring JSON、不调用 compiler/provider，也不创建第二套 package writer 或 validator。响应只包含完整二进制、长度和安全 `Content-Disposition` 文件名；provider generation/checksum、package SHA、CAS 与内部路径不进入 HTTP 或 DOM。

文件名仅从当前 workspace 相对 JSON 的 ASCII-safe basename 派生；非法、空、设备名或过长 basename 固定回落为 `sandbox-package.tgdsbx`。文件名不参与 package identity。存在未应用字段、冲突、非 `ready` 状态或任一检查/导出在途时，导出按钮失败关闭；旧 validation/bridge/stale 状态即使保留上一份包也不能下载。浏览器只在完整响应仍属于同一 UI epoch 时创建下载，成功文案“已交给浏览器下载；尚未启动 Preview 或试玩”不宣称磁盘保存成功。

## 保存协议

1. Open 对磁盘原始字节计算 SHA-256 opaque CAS，并只在 authoring `1.1.0` 解析成功后替换当前状态。
2. Save 同时检查浏览器 `expectedRevision`、当前 controller CAS 和替换前重新读取的磁盘 CAS。
3. 写入使用目标同目录的 `wx` 临时文件，完成 write、file sync、close 后执行同卷 rename 替换。
4. 任一写入、sync、CAS 或 replace 失败都会尝试删除临时文件；原文件不变，dirty 不会清除。
5. 替换成功后才尝试 `document.mark_saved`。保存期间若 revision 前进，磁盘保存旧快照，内存中的新 revision 继续 dirty。
6. 外部修改造成 CAS 冲突时不覆盖；dirty 状态下 Open/Reload 必须由用户确认丢弃。

路径拒绝绝对路径、盘符、UNC、null、`.`、`..`、Windows 设备名、目录、最终 symlink，以及 realpath 后逃出 workspace root 的 reparse/junction 路径。

### 已知 Windows 竞态

进程内目标锁可以串行化所有 Workbench 写入，替换前 CAS 可以阻止已观察到的外部修改；纯 Node 文件 API 不能阻止不合作进程恰好在最终 CAS 重读和 rename 之间抢写。该极窄竞态必须由后续 PLATFORM 原生锁/替换能力评估，当前实现不会把它描述成系统级事务。

## 测试

```powershell
npm --prefix apps/content-workbench test
npm --prefix apps/content-workbench run test:browser
npm test
npm run validate:design
npm run check:web-abi
npm run check:f1-slice-contract
git diff --check
```

浏览器门使用真实 Playwright Chromium、loopback 随机端口和临时 workspace，不使用 JSDOM。

## Remaining Open

- GAME-003：在已完成的 package document→standalone Session adapter 之上，建立 package/provider/Session/Collision/Assets 的整体候选与回滚。
- PLATFORM：Windows/Web Preview、整包运输与显式 reload。
- TOOLS：2D 画布、对象增删、waves/objectives 编辑和 Preview 请求；Launch/Reload 必须等待 PLATFORM Host 与资产 resolver。
