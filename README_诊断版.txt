Inkpath 三阶段图像处理诊断版

修改文件：src/plugin/plugin.cpp、src/plugin/plugin.h、src/cv/inkpath_config.h、plugin/main.lua、plugin/config.ini。
其它文件保持来源版本不变。可在原源码根目录覆盖完整目录结构。

启用：已安装插件目录 config.ini 的 [diagnostics] 下 enabled = 1。
关闭：enabled = 0。每次导入重新读取，无需再次编译。

当诊断开启时，图片导入后弹窗显示修复前后像素变化数及保存目录。
默认存放 %TEMP%\InkpathDiagnostics\：
01_threshold.png  二值化结果、修复前
02_repaired.png   修复后的二值结果
03_skeleton.png   骨架化后结果
三张图像会在下一次诊断时覆盖。关闭时不生成图像或弹窗。

MSYS2 MINGW64：
cd /e/Local/mingw64/inkpath
cmake -S . -B build-new
cmake --build build-new --parallel 4

必须将新 DLL、main.lua、config.ini 部署到 Xournal++ 的 ImageTranscription 目录。
若改动 config.ini 中开关，只需保存并再次导入；不用重新编译。

注意：诊断统计的是修复前后整个二值图发生变化的像素数，数值大于 0 不代表断裂笔画一定被正确连接。关闭 repair 时变化数量应为 0。
