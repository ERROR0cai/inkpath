Inkpath runtime config (for your existing modified Windows build)
=============================================================
文件树：
  CMakeLists.txt                    原 CMake 配置（保留）
  plugin/config.ini                 新增统一运行时配置
  plugin/main.lua                   修改：每次导入重新读取输出参数
  plugin/plugin.ini                 保留原 Xournal++ 插件入口
  plugin/clipboard_to_png.ps1       保留原 Windows 剪贴板工具
  src/cv/inkpath_config.h           新增：C++ INI 读取、默认值、参数检查
  src/cv/ipcv.h                     修改：图像处理参数传递声明
  src/cv/ipcv.cpp                   修改：高斯、二值化、骨架、轮廓参数
  src/cv/quadrangle.cpp             修改：白板检测参数
  src/plugin/plugin.cpp             修改：每次转换读取配置、按参数处理
  src/plugin/plugin.h               保留原有声明

首次部署步骤：
1. 备份 E:\Local\mingw64\inkpath 的源文件。
2. 将上述各文件按相对路径覆盖到 E:\Local\mingw64\inkpath。
   原有其它文件、DLL 和依赖保持不动。
3. 打开 MSYS2 MINGW64：
   cd /e/Local/mingw64/inkpath
   cmake -S . -B build-new
   cmake --build build-new --parallel 4
4. 完全退出 Xournal++。PowerShell 执行：
   $root = 'E:\Local\mingw64\inkpath'
   $target = 'E:\tools\Xournal++\share\xournalpp\plugins\ImageTranscription'
   $backup = "$target.backup-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
   Copy-Item $target $backup -Recurse -Force
   Copy-Item "$root\build-new\ImageTranscription\libinkpath.dll" "$target\libinkpath.dll" -Force
   Copy-Item "$root\plugin\main.lua" "$target\main.lua" -Force
   Copy-Item "$root\plugin\config.ini" "$target\config.ini" -Force
5. 启动 Xournal++，先用测试文档验证普通图片、强制黑白、剪贴板以及下一页。

以后调整参数（无需编译，无需重启）：
- 直接编辑安装目录里的 config.ini：
  E:\tools\Xournal++\share\xournalpp\plugins\ImageTranscription\config.ini
- 保存之后，下次导入就读取新参数。
- 注意：只修改源码目录 plugin/config.ini，不会自动更新已经安装的 config.ini；
  源码与安装目录是两份文件，需要自行同步。
- 菜单里的两个强制黑白项始终覆盖 image.polarity_mode。

兼容性 / 限制：
- 核心图像算法不更换，只将现有参数转成可配置。
- 缺失 config.ini 使用内置默认值；错误配置会报错，不会主动 exit()。
- C++ 配置参数按每次图片识别读取；Lua 输出参数也按每次导入读取。
- 本环境已测试 C++ 配置解析器与 Lua 解析语法，但没有本机 Windows OpenCV 5、Lua 5.5
  与 Xournal++ 运行环境，因此不能替代在你的 MSYS2 环境里完整编译、实测。
