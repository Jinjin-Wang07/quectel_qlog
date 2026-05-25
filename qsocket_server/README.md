# QSOCKET LOG 使用指南
## 一.快速运行
 1. 检查局域网是否正常
 2. 检查 __QSocketLog__ 编译是否报错
 3. 运行 __QSocketLog__ 抓取日志

### 检查局域网是否正常
第一步：查看运行 __QSocketLog__ 主机(下面称PC)的 __ip__ 地址,可通过 "ifconfig" 指令查看

第二步：通过 __ping__ 工具确认网络状态

### 编译 __QSocketLog__ 工具
第一步： 进入 __qsocket_server__ 目录，运行 "make" 指令来编译源码，如果你使用特定的编译器，请修改 "Makefile" 里面的 "CC" 变量

第二步： 如果编译没有报错，生成的只执行文件在 __bin__ 目录中。

### 运行 __QSocketLog__ 
第一步：
'''
    ./QSocketLog -f demo.cfg -s ip-port
'''
其中：
    -f :使用特定的cfg文件，用来过滤模组中的log
    -s :连接模组内的socket server端
        ip:socket server端的IP地址
        port:socket server端的端口号，默认14000
    -h :help,可通过此参数查看其它参数的使用方法

The End