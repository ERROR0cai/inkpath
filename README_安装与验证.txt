Inkpath 保守去噪与密度自适应解耦补丁

本包根据用户上传的当前文件生成。保留其他源码与参数数值。

改动：
1. src/cv/ipcv.cpp：density_enabled=0 时，锚点门槛和候选范围只依赖 max_component_pixels，不再读取 sparse_max_pixels；密度为 1 时才使用 sparse_max_pixels。密度窗口本来就只在启用时计算，继续保持。
2. src/cv/inkpath_config.h：仅在 density_enabled=1 时检查 sparse_max_pixels >= max_component_pixels。数值类型和各自合法范围仍然检查。
3. plugin/config.ini：更新三项中文注释，不更改任何参数值。
4. 其他文件仅按上传版原样保留。

部署：将 src/cv/ipcv.cpp、src/cv/inkpath_config.h 覆盖到项目同名路径；其他源码本包也有，若未做本地额外修改可完整覆盖。
在 MSYS2 MINGW64：
cd /e/Local/mingw64/inkpath
cmake -S . -B build-new
cmake --build build-new --clean-first --parallel 4
关闭 Xournal++ 后，只需把 build-new/ImageTranscription/libinkpath.dll 更新到已安装插件目录。config.ini 可参考包中注释手动更新，避免覆盖自己的本地设置。

验证：固定同一输入图像和其他参数；设置 density_enabled=0，将 sparse_max_pixels 分别设 6、12、100，输出应逐像素相同（包含关闭 diagnostics 时最终笔迹）；再设 density_enabled=1，验证大小关系检查生效。注意 sparse_max_pixels 仍须在单项许可范围 0..10000。
