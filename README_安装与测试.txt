Inkpath 图结构中心线重建实验版
================================
版本性质：几何拓扑启发式实验版，不是 OCR；不能保证从静态图精确恢复原始笔顺、X/Z/Q 的笔数。

文件说明
src/cv/stroke_graph.cpp：新增 Zhang-Suen 细化与骨架图路径重建（务必复制！）
src/cv/ipcv.h：新增声明
src/cv/inkpath_config.h：新增 reconstruction 配置读取
src/plugin/plugin.cpp：接入图模式、诊断图片 06/07
plugin/config.ini：保留用户现有参数，追加 [reconstruction]
其他源码未变或保留以便完整部署。

编译前：将压缩包中的 src 和 plugin 对应文件覆盖至 E:\Local\mingw64\inkpath 中。
注意：不要删除用户原来的 cmake 文件夹或已有构建环境。

MINGW64:
cd /e/Local/mingw64/inkpath
cmake -S . -B build-new
cmake --build build-new --clean-first --parallel 4

退出 Xournal++ 后在 PowerShell 部署：
$source='E:\Local\mingw64\inkpath'
$target='E:\tools\Xournal++\share\xournalpp\plugins\ImageTranscription'
Copy-Item "$source\build-new\ImageTranscription\libinkpath.dll" "$target\libinkpath.dll" -Force
Copy-Item "$source\plugin\main.lua" "$target\main.lua" -Force
# 建议备份原 config.ini，再复制新 config.ini，否则将缺少 [reconstruction] 可调参数。
Copy-Item "$source\plugin\config.ini" "$target\config.ini" -Force

[reconstruction] mode=GRAPH ：新的中心线图提取。mode=CONTOUR ：旧 findContours 回退。
GRAPH 自动绕过旧 [repair] 闭运算，即使 repair.enabled=1；旧闭运算仅在 CONTOUR 工作。
GRAPH 默认 thinning=ZHANG_SUEN。保守后处理去噪保持独立运行。
注意：GRAPH 模式并不能真正识别字符边界，character_guard 是局部尺度启发式。

诊断：在 [diagnostics] 设置 enabled=1，重新导入图片。输出在 %TEMP%\InkpathDiagnostics：
01_threshold.png, 02_repaired.png, 03_skeleton.png, 04_denoised.png, 05_removed_mask.png,
06_graph_added_mask.png（桥接新增位置白色），07_reconstructed.png（路径渲染）

优先测试 X、Z、Q、小数点和多个相邻汉字；核对是否误连、误删。
出现误连：先减小 max_gap=4、corner_max_gap=2、smooth_angle=25，或 gap_enabled=0。
如不满意：mode=CONTOUR 可回退旧轮廓算法，无需重编译（下一次导入生效）。

限制：未在用户 Windows / OpenCV5 / Lua5.5 环境编译验证；不保证笔画数和字间边界正确。
