## 网络文件管理器(Cloud Disk)

作者： `董涛`

介绍 ：

网络文件服务器是基于 TCP/IP 协议开发的文件管理系统， 服务器通过 mysql 服务器管理用户信息。所以服务器的基本安装需要有mysql的连接驱动，以及C编译工具gcc.

本软件采用多线程维护服务器跟用户事件处理。在事件处理过程中 用户事件在mysql中记录日志，跟维护用户基本信息。 系统信息维护在服务器日志文件中。用户一定时间不操作将会自动下线。

此软件仅作交流学习为目的开发， 对本软件有更好的意见或建议欢迎联系作者交流。

更加详细的信息请阅读 docs 目录下文档。

## 如何安装:

### 1. 准备条件：

1. mysql的连接驱动程序

2. gcc 编译工具

3. mysql服务器

### 2. mysql 服务器准备 :


1. 新建用户 `saligia` 密码 `360628836989`

2. 建立数据库 : `saligia`

3. 在 saligia 数据库中建表

```
--- 用户信息维护表
CREATE TABLE `usermessage` (
  `u_id` int(11) NOT NULL AUTO_INCREMENT,
  `u_name` varchar(50) NOT NULL,
  `u_passwd` varchar(25) NOT NULL,
  `u_ltime` datetime NOT NULL,
  `u_flag` tinyint(4) NOT NULL,
  PRIMARY KEY (`u_id`)
) ENGINE=InnoDB AUTO_INCREMENT=1000000 DEFAULT CHARSET=utf8

--- 用户日志记录表
CREATE TABLE `userlog` (
`l_time` datetime NOT NULL,
`l_id` int(11) NOT NULL,
`l_content` varchar(200) DEFAULT NULL,
KEY `id_user_ref` (`l_id`),
CONSTRAINT `id_user_ref` FOREIGN KEY (`l_id`) REFERENCES `usermessage` (`u_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8

```

5. 环境检测

```
./Configuration
```

6. 编译
```
make
```
7. 安装
```
make install
```

执行方式:

```
service safile start
```

## 作者信息:

```
邮箱 : gg782112163@163.com
QQ   : 782112163
```
