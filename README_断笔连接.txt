Inkpath 微小断口连接：完整源码改动版

改动文件：
  src/cv/inkpath_config.h   新增 repair 配置解析
  src/plugin/plugin.cpp     二值化与骨架提取之间增加可选闭运算
  plugin/config.ini        添加中文 repair 配置
  plugin/main.lua          使用用户最新的无插值版本

保持其余文件与已提供的“配置化完整版”源码一致。
默认 repair.enabled = 0：安装后仍采用原处理流程。
建议首次试验设置：
[repair]
enabled = 1
kernel_size = 3
iterations = 1
kernel_shape = ELLIPSE

注意：连接操作作用于二值化阶段，能够填补小断口，但不可能恢复未被阈值识别出来的整段淡粉笔笔画。
较大的核会导致相邻字符连接，尤其对小数点、上下标、分数符号需要谨慎。

构建（MSYS2 MINGW64）：
cd /e/Local/mingw64/inkpath
cmake -S . -B build-new
cmake --build build-new --parallel 4

关闭 Xournal++ 后，将 build-new/ImageTranscription/libinkpath.dll 复制到已安装插件目录；
将 plugin/config.ini 和 plugin/main.lua 复制到相同目录。
如果源码曾有本地变更，请先备份并对比差异，不要直接用完整压缩包覆盖其他本地改动。
以后调节 repair 参数不需要重编译；这次增加 C++ 功能必须重编译一次。
