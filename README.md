# buffer_pool_control_plugin

## 快速开始
### 编译插件
```
# 下载mysql源码
git clone https://github.com/mysql/mysql-server.git -b mysql-5.7.35
# 进入插件目录
cd mysql-server/plugin
# 下载插件代码
https://github.com/shenkonghui/buffer_pool_control_plugin.git

# 进入源码根目录,cmake源代码
cd mysql-server
cmake .

完成后进入插件生成的目录make，得到插件buffer_pool_control.so
cd mysql-server/cmake-build-debug/build/plugin/buffer_pool_control
make
```
### 安装插件

```
# 连接mysql，并查看插件目录
SHOW GLOBAL VARIABLES LIKE 'plugin_dir';

# 移动插件到目录
cp buffer_pool_control.so xxx

# 安装插件
mysql> INSTALL PLUGIN buffer_pool_control SONAME "buffer_pool_control.so";
```

### 使用
```
# 查看配置，默认不开启
mysql> show variables like "%buffer_pool_control%";
+------------------------------+-----------+
| Variable_name                | Value     |
+------------------------------+-----------+
| buffer_pool_control_enabled  | OFF        |
| buffer_pool_control_interval | 10        |
| buffer_pool_control_min      | 134217728 |
| buffer_pool_control_ratio    | 0.700000  |
+------------------------------+-----------+
#  开关
mysql> set global  buffer_pool_control_enabled=1;
Query OK, 0 rows affected (0.00 sec)

# 查看innodb_buffer_pool_size是否调整，默认设置为内存的70%; 容器中会根据limit值进行设置
mysql> show variables like "%innodb_buffer_pool_size%";
+-------------------------+------------+
| Variable_name           | Value      |
+-------------------------+------------+
| innodb_buffer_pool_size | 2887778304 |
+-------------------------+------------+
1 row in set (0.01 sec)
```
