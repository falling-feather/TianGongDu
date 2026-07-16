# Sandbox Content Workbench

这是 `TOOLS-001.5A` 的最小可操作编辑器：在一个启动时固定的 workspace 中打开既有 `tgd.sandbox.authoring` `1.1.0` JSON，浏览并编辑 player、actors、groundBlockers、safePoints、interactions、mechanisms，然后以 CAS 保护保存或显式重载。

## 边界

- Workbench 只拥有作者草稿、浏览器表单、revision/dirty/last-valid 协调和本地 workspace I/O。
- Stable ID 只读；本切片不新增、删除或改名对象，也不提供通用 JSON/target 编辑器。
- JavaScript author shape 只是非权威结构门。页面中的“结构草稿”不表示通过 DEV validator、可导出或可玩。
- 本切片没有 compiler/provider bridge、共享 diagnostics、`.tgdsbx` producer、Export、GAME Session、Windows/Web Preview、F1/Profile/奖励或任意命令执行。
- 页面永久使用“字段格式可保存；尚未进行内容与玩法校验。”，不会向用户暴露 CAS、hash、JSONPath 或技术异常。
- 六类对象共用一个键盘 Tab-stop tree；对象字段缓冲按对象保留，Escape 撤销当前字段，存在任意未应用缓冲时 Save 禁用。
- 外部修改不会抢焦；处理对话框由用户主动打开，首焦为“继续编辑”，取消或 Escape 返回触发控件。

## 运行

需要 Node.js 20 或更高版本。服务器固定绑定 `127.0.0.1` 的随机端口；workspace root 可以是绝对路径，但浏览器中的文档路径必须是该 root 内的既有相对 `.json` 文件。

```powershell
npm --prefix apps/content-workbench start -- --workspace "D:\path\to\sandbox-workspace"
```

服务器会打印一次本地 URL。它只公开三项静态资源和有限的 open/reload/update/save/state API，不启动浏览器，也不执行外部命令。

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

- DEV-002：authoring runtime projection 到唯一 validator/producer、owning package document 与共享 diagnostics。
- GAME-003：package document 到 Sandbox Session adapter。
- PLATFORM：Windows/Web Preview、整包运输与显式 reload。
- TOOLS：2D 画布、对象增删、waves/objectives 编辑、compiler adapter、导出和 Preview 请求。
