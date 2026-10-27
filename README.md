# Inkpath — Xournal++ 图片转可编辑笔迹（Windows 增强分支）

> 基于 [WillNilges/inkpath](https://github.com/WillNilges/inkpath) 改进的社区分支。项目仓库：[ERROR0cai/inkpath](https://github.com/ERROR0cai/inkpath) · [dev 分支](https://github.com/ERROR0cai/inkpath/tree/dev) · [Releases](https://github.com/ERROR0cai/inkpath/releases)。
>
> 主要在 **Windows / MSYS2 MINGW64 / OpenCV 5 / Lua 5.5** 环境下进行开发与编译，在 **Xournal++ 1.3.8** 版本完成基本功能测试。Linux 原有脚本保留，但该分支的 Linux 构建与运行兼容性尚未系统验证。

## 项目介绍

Inkpath 将白板、黑板、纸面手写笔记的照片或截图转成 Xournal++ 中可单独选择、删除和编辑的**笔迹路径**，而不是插入一张不可编辑的图片。它是基于图像处理的笔迹提取工具，**不是 OCR 或手写文字识别器**；生成的路径不保证符合真实笔顺或准确恢复每个字符的笔画数。

本分支保留上游图像转笔迹的基本目标，重点增加运行时参数配置、Windows 剪贴板导入、去噪、诊断、可切换的骨架图重建算法，以及近共线干扰线段清理；近期版本还修复了原生接口的潜在内存安全问题，并优化了大量笔迹时的数据读取性能。

## 转换效果展示

将拍摄的手写笔记图片转换为 Xournal++ 内可编辑的独立笔迹路径。以下为同一内容转换前后的示例，实际效果受照片质量、光照、阈值及参数配置影响。

**转换前：拍摄的原始笔迹**

<img width="2432" height="836" alt="原始拍照笔迹" src="https://github.com/user-attachments/assets/fad27579-fcfc-448d-98de-0491db260e54" />

**转换后：Xournal++ 中的可编辑笔迹**

<img width="1882" height="659" alt="转换后的可编辑笔迹" src="https://github.com/user-attachments/assets/665ad39a-1d00-4e5f-9da6-5286e66b9336" />

转换结果不是文字识别结果：每条路径可单独选择、移动或删除，但不保证恢复真实笔顺。

## 近期版本更新

### v0.2.0-dev.3 — 内存安全与性能优化

- **修复潜在越界访问**：为笔迹索引加入边界检查，避免非法索引直接访问 C++ 容器。
- **强化对象生命周期管理**：改进原生对象释放、空指针检查及 Lua/C++ 接口的错误处理与参数验证。
- **消除重复复制**：使用常量引用读取笔迹集合，并通过移动语义减少数据复制和分配开销。
- **提升复杂图片处理速度**：大量笔迹场景下的数据传递更高效，实际加速效果取决于图片和笔迹数量。
- **兼容原有功能**：保留原有识别、重建、去噪、线段过滤和诊断流程；Windows + MinGW64 下已成功编译。长期稳定性及此前退出时的崩溃问题仍需继续验证。

### v0.2.0-dev.2 — 近共线边缘线段清理

- **新增近共线多段清理**：识别由多条近似共线线段构成的干扰笔迹，兼容水平、竖直及倾斜方向。
- **优化连续子组识别**：沿共同直线划分连续子组，避免不合格线段干扰其他候选线段的清理。
- **新增独立长直线清理**：使用 `single_enabled` 和 `single_min_length` 控制单条长直线过滤。
- **修复删除逻辑**：单条与多段线段分别判断、统一删除，避免阈值调整导致部分线段意外重新出现。
- **完善配置与诊断**：通过 `[edge_lines]` 设置边缘范围、长度阈值、断口距离、方向容差，并使用 `08_edge_lines_removed.png` 检查过滤结果。

**注意：** `[edge_lines]` 默认关闭，设为 `enabled = 1` 后才启用。几何过滤可能误删分数线、坐标轴等正常笔迹，建议先开启诊断并使用测试文档验证。

## 相对于上游的主要变化

| 功能 | 本分支实现 |
| --- | --- |
| 运行时配置 | 新增 `plugin/config.ini`；C++ 和 Lua 在导入时读取相关配置，无须为普通参数调整重编译 |
| 图结构笔迹重建 | 新增 `src/cv/stroke_graph.cpp`，使用骨架像素图追踪线段、交点方向配对及短断口连接 |
| 骨架细化 | `ZHANG_SUEN` 与旧 `MORPH` 方法可选 |
| 回退模式 | `reconstruction.mode = CONTOUR` 可选择旧轮廓提取方法 |
| 断口修复 | `gap_enabled`、最大距离、方向角、折角和局部尺度约束；GRAPH 模式不使用旧形态学闭运算 |
| 去噪 | 骨架连通域的保守去噪、邻近有效笔迹保护和局部密度自适应 |
| 近共线边缘线段清理 | 可选的多段近共线干扰线清理与独立长直线过滤，支持任意方向及诊断预览 |
| 笔迹数据性能优化 | 使用常量引用和移动语义，减少 C++/Lua 接口读取大量笔迹时的重复复制 |
| 原生接口安全加固 | 改进笔迹索引检查、Lua 参数验证及对象生命周期管理 |
| 诊断 | 可输出各处理阶段的图像，便于区分阈值化、细化、去噪或修复产生的问题 |
| Windows 剪贴板 | 通过 PowerShell 辅助脚本将剪贴板图像导出后转换 |
| 导入位置 | 可配置页面缩放、偏移，以及导入当前页或下一页 |
| 背景极性 | 自动检测或在菜单中强制选择白底黑字、黑底白字 |

上述算法均属于几何启发式处理；“交点配对”不等于识别文字，也不能保证 X、Z、Q 或相邻汉字绝不误连。

## Windows 安装（使用已构建的发行包）

1. 在 [Releases](https://github.com/ERROR0cai/inkpath/releases) 下载本分支提供的 **Windows 插件二进制包**（如发行版已附带）。GitHub 自动生成的 `Source code (zip)` **不是**可直接安装的 DLL 包。
2. 完全退出 Xournal++。
3. 将发行包中的 `ImageTranscription` 插件文件夹放入 Xournal++ 的 `share/xournalpp/plugins/` 下。安装路径因机器而异，例如 `E:\tools\Xournal++\share\xournalpp\plugins\ImageTranscription`。
4. 确保文件夹中至少有 `main.lua`、`plugin.ini`、`getLibPath.lua`、`clipboard_to_png.ps1`、`config.ini`、`libinkpath.dll`，以及该 DLL 在目标机器上所需的**兼容运行库 DLL**。
5. 启动 Xournal++，使用 Inkpath 菜单导入测试图片。

**运行库注意事项：** 本分支在 Windows 上动态链接 OpenCV、Lua 和 MinGW 相关运行库。请从相同工具链构建环境收集并测试所需依赖，不能仅把 `libinkpath.dll` 当成完全静态、独立的插件。不要盲目复制整个 `build-new` 构建缓存作为发行包。已有其他版本插件时，先备份原安装目录和 `config.ini`。

## Windows 源码构建

环境：MSYS2 **MINGW64**、CMake、MinGW-w64 C++ 编译器、OpenCV（已使用 5.x 构建）、Lua **5.5** 及相应头文件/链接库。实际依赖名称请以 `CMakeLists.txt` 和本机包管理器为准。确认 `cmake`、`g++`、`pkg-config` 等指向同一 MINGW64 工具链。

在 **MSYS2 MINGW64** 终端：

```bash
cd /e/Local/mingw64/inkpath
cmake -S . -B build-new -G "MinGW Makefiles"
cmake --build build-new --parallel 4
```

编译成功标志：`[100%] Built target inkpath`。DLL 通常位于 `build-new/ImageTranscription/libinkpath.dll`。

**注意：** 当前 CMake 已取消构建后自动拷贝 MinGW 运行库 DLL 的步骤。源码构建成功不等于 Windows 插件包依赖已经齐全。部署时应将其与对应的 `plugin/` 文件及必要依赖一起安装。

完全退出 Xournal++ 后，可用 PowerShell 更新已有插件（请根据实际目录修改路径）：

```powershell
$root = 'E:\Local\mingw64\inkpath'
$dst  = 'E:\tools\Xournal++\share\xournalpp\plugins\ImageTranscription'
Copy-Item "$root\build-new\ImageTranscription\libinkpath.dll" "$dst\libinkpath.dll" -Force
Copy-Item "$root\plugin\main.lua" "$dst\main.lua" -Force
Copy-Item "$root\plugin\clipboard_to_png.ps1" "$dst\clipboard_to_png.ps1" -Force
# 如需同步项目默认配置，请先备份已安装的 config.ini：
# Copy-Item "$root\plugin\config.ini" "$dst\config.ini" -Force
```

这段操作适用于**已有完整运行库和插件资源**的安装目录，不能代替首次部署完整依赖。

## 使用方法

在 Xournal++ 的 Inkpath 菜单中：
<img width="594" height="269" alt="image" src="https://github.com/user-attachments/assets/ff6dc59e-f76a-46a6-8219-dc408cf2d472" />


- `Inkpath - From image file`：选择 JPG、JPEG、PNG、BMP，按自动极性配置转换；快捷键 `Ctrl+Alt+T`。
- `Inkpath - Force white background / black strokes`：强制按白底黑笔迹解析。
- `Inkpath - Force black background / white strokes`：强制按黑底白笔迹解析。
- `Inkpath - From clipboard image`：Windows 剪贴板图片导入；快捷键 `Ctrl+Alt+G`。剪贴板必须包含系统可识别的**图像**，仅复制图片文件的路径不一定有效。

默认输出行为由 `plugin/config.ini` 中 `[output]` 控制。`next_page = 1` 表示将笔迹加入当前页的下一页；`0` 表示当前页。导入不是 OCR，建议先在测试文档验证位置、笔画拆分和符号保留情况。

## 参数配置

修改的是**已安装插件目录**中的 `config.ini`；修改源码目录中的 `plugin/config.ini` 不会自动同步到已安装版本。保存后，通常下一次导入即读取新配置。

主要配置节如下：

| 节名 | 作用 |
| --- | --- |
| `[output]` | 缩放、偏移、最少路径点数和目标页 |
| `[image]` | 图像最长边缩放与黑白极性 |
| `[perspective]` | 可选透视校正；`passes=0` 时跳过 |
| `[threshold]` | 高斯模糊与自适应二值化 |
| `[repair]` | 旧轮廓模式的形态学断口闭运算 |
| `[skeleton]` | 旧形态学骨架使用的结构元素 |
| `[contours]` | 旧轮廓检索和轮廓近似选项 |
| `[diagnostics]` | 是否保存中间图像和输出处理报告 |
| `[denoise]` | 保守连通域清理、邻近保护和密度去噪 |
| `[reconstruction]` | GRAPH/CONTOUR、细化、交点配对与断口连接 |
| `[edge_lines]` | 近共线多段清理、独立长直线清理及边缘范围、方向和距离阈值 |

示例（只展示关键选项，并非覆盖你当前设置的建议）：

```ini
[reconstruction]
mode = GRAPH
thinning = ZHANG_SUEN
gap_enabled = 1
max_gap = 7
junction_pairing = 1
character_guard = 1
```

使用 `GRAPH` 时跳过旧 `[repair]` 闭运算，使用 `CONTOUR` 时不执行图结构笔迹重建。`max_gap` 在当前配置解析器中有效范围为 **0～50**；超出会报告 `Out-of-range value for reconstruction.max_gap`。

两个常见的参数关联：

- `GRAPH` 模式的最终最少点数同时受 `reconstruction.min_points` 与 `output.min_stroke_points` 限制。
- 开启密度自适应去噪时，有效锚点像素阈值至少为 `max(anchor_min_pixels, sparse_max_pixels + 1)`；关闭时不应让稀疏区配置干扰普通去噪判定。

近共线线段清理配置示例（默认不启用，参数值仅作示意，实际阈值应按图片调整）：

```ini
[edge_lines]
enabled = 0
single_enabled = 0
single_min_length = 300
```

`enabled = 1` 启用边缘线段过滤；`single_enabled` 决定是否清理符合阈值的独立长直线。过滤不限于水平、竖直方向，对倾斜线段也适用。具体参数及可用取值以当前 `config.ini` 为准。

完整取值范围和参数联动说明请阅读 [`plugin/config.ini`](plugin/config.ini) 的中文注释。

## 诊断与故障排查

把 `[diagnostics] enabled` 设为 `1`，重新导入图像。诊断结果通常位于 `%TEMP%\InkpathDiagnostics`：

| 文件 | 意义 |
| --- | --- |
| `01_threshold.png` | 二值化结果 |
| `02_repaired.png` | 旧断口闭运算之后的结果（GRAPH 模式通常跳过该步骤） |
| `03_skeleton.png` | 骨架细化结果 |
| `04_denoised.png` | 连通域去噪结果 |
| `05_removed_mask.png` | 去噪删除像素的掩码 |
| `06_graph_added_mask.png` | GRAPH 模式新增断口连接像素的掩码 |
| `07_reconstructed.png` | GRAPH 模式的重建中心线示意 |
| `08_edge_lines_removed.png` | 启用近共线线段清理时，显示被过滤的候选笔迹 |

如果开启直线过滤后误删分数线、坐标轴或字形线条，先检查 `08_edge_lines_removed.png`，再调整 `[edge_lines]` 的阈值或关闭该功能。

如果小数点、上下标丢失，先检查阈值化与 `05_removed_mask.png`；若出现跨字符误连，检查 `06_graph_added_mask.png` 并酌情降低断口距离、角度容忍度或关闭 `gap_enabled`；如果仅对图重建结果不满意，可以用 `reconstruction.mode=CONTOUR` 对照旧模式。

**稳定性说明：** v0.2.0-dev.3 修复了已发现的部分原生内存安全隐患，但不能据此认定所有崩溃问题均已解决。对大量笔迹文档，请先备份并测试导入、撤销、重做、保存和退出流程。

**其他已知限制：** 交点配对、断口连接和字符尺度保护均属启发式规则；可能产生额外碎线、错误连接或与人工书写笔顺不同的路径。当前以 Windows 为主要验证环境，不保证跨系统 ABI/依赖可直接复用。

## 源码结构

```text
plugin/
  main.lua                 # Xournal++ 菜单、文件/剪贴板导入、输出页面
  clipboard_to_png.ps1     # Windows 剪贴板图像导出
  config.ini               # 中文详细参数配置
  plugin.ini               # Xournal++ 插件清单
  getLibPath.lua           # 上游保留辅助模块
src/cv/
  inkpath_config.h         # C++ INI 解析、默认值与参数校验
  ipcv.cpp, ipcv.h         # 图像二值化、骨架及相关操作
  quadrangle.cpp           # 透视/四边形检测
  stroke_graph.cpp         # Zhang–Suen 细化与图结构路径重建
src/plugin/
  plugin.cpp, plugin.h     # C++/Lua 插件接口及处理流程
HACKING/, scripts/        # 上游开发与构建工具
CMakeLists.txt             # 构建定义
```

`.git/`、`.vs/`、`build-new/` 均不是应放进源码发行压缩包的组成部分；其中 `build-new/ImageTranscription/` 可作为打包 Windows 二进制的**素材**，但需独立核实和精简运行依赖。

## 上游与许可证

本项目是 [WillNilges/inkpath](https://github.com/WillNilges/inkpath) 的衍生版本，不代表上游官方发布，也不声称是全新独立开发项目。感谢上游作者及 Xournal++、OpenCV 社区。

请保留原仓库的 `LICENSE`、源代码许可声明及上游归属信息。仓库当前包含 GNU GPL v2 许可证文本；再分发源码和二进制时应按实际适用许可证履行相应义务，包含必要的源代码或获取途径与版权说明，并核对各动态依赖的许可证。
