Inkpath 保守去噪与密度自适应版

1. 先备份源码和 Xournal++ 安装目录。解压后按照相对路径覆盖项目源代码：
   E:\Local\mingw64\inkpath\src\cv\
   E:\Local\mingw64\inkpath\src\plugin\
   E:\Local\mingw64\inkpath\plugin\

2. 本版沿用既有接口，没有修改工程的 CMakeLists.txt，不需要安装额外依赖。
   在 MSYS2 MINGW64：
   cd /e/Local/mingw64/inkpath
   cmake -S . -B build-new
   cmake --build build-new --clean-first --parallel 4

3. 完全关闭 Xournal++，在 Windows PowerShell：
   $src = 'E:\Local\mingw64\inkpath'
   $dst = 'E:\tools\Xournal++\share\xournalpp\plugins\ImageTranscription'
   Copy-Item "$src\build-new\ImageTranscription\libinkpath.dll" "$dst\libinkpath.dll" -Force
   Copy-Item "$src\plugin\main.lua" "$dst\main.lua" -Force
   Copy-Item "$src\plugin\config.ini" "$dst\config.ini" -Force
   注意：这最后一步会覆盖你已有的 config.ini，请自行合并或先备份。

4. [denoise] 新参数均有中文说明。保守模式默认启用 (enabled=1)，密度自适应默认关闭 (density_enabled=0)。
   如果需要与旧版对比，将 enabled=0；其他功能不受影响。
   修改已安装的 config.ini 后，下次导入生效，不需要重复编译。

5. [diagnostics] enabled=1 时输出（%TEMP%\InkpathDiagnostics）：
   01_threshold.png  修复前二值图
   02_repaired.png   断口修复后二值图
   03_skeleton.png   去噪前骨架
   04_denoised.png   去噪后骨架
   05_removed_mask.png  白色像素是被删除的部分
   弹窗中报告删除的连通域和像素数量。

说明：连通域大小、邻近保护、密度自适应都在骨架图像上工作；一张文字照片在骨架化阶段
可能产生大量小碎片，因此本算法不能保证所有删除均为真正噪点。请先开启诊断查看删除掩码，
确认没有误删小数点、上标等关键内容。软件构建与 Xournal++ 环境仍需在用户机器上验证。
