# QLOG USER GUIDE
## FOR UNISOC EC200U
NOTE: 如果你想压缩日志，可以通过以下几个步骤来达到目的
1. 切换工作目录到QLog
    eg:cd /home/q/QLog_Linux_and_Android_V1.5.27

2. 编译工具
    make

3. 将压缩脚本拷贝到 __out__ 目录
    cp test/qcompress.sh out/
    chmod a+x qcompress.sh

4. 进入 __out__ 目录，并执行 __QLOG__
    cd out
    sudo ./QLog -m 5 -C 50
    -m 是每个文件大小到 __5MB__ 就会压缩
    -C 启动压缩功能，并设置log总量位 __50MB__

5. 如果想更改压缩指令，可以在 __qcompress.sh__ 第 __27__ 行更改，改完以后不需要重新编译 __QLOG__ 工具