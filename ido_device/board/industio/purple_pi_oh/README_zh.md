# 【开发板名称】Purple Pi OH开发套件

**简介**

触觉智能urple Pi OH是基于Rockchip
RK3566，集成双核心架构GPU以及高效能NPU；板载四核64位Cortex-A55
处理器采用22nm先进工艺，主频高达2.0GHz；支持蓝牙、Wi-Fi、音频、视频和摄像头等功能，拥有丰富的扩展接口，支持多种视频输入输出接口；配置双千兆自适应RJ45以太网口，可满足NVR、工业网关等多网口产品需求。


### **搭建开发环境**

#### **1、安装依赖包**

安装命令如下：

```
sudo apt-get update && sudo apt-get install binutils git git-lfs gnupg flex bison gperf build-essential zip curl zlib1g-dev gcc-multilib g++-multilib libc6-dev-i386 lib32ncurses5-dev x11proto-core-dev libx11-dev lib32z1-dev ruby ccache libgl1-mesa-dev libxml2-utils xsltproc unzip m4 bc gnutls-bin python3-pip
```

```
sudo apt-get install device-tree-compiler
sudo apt-get install libssl-dev
sudo apt install libtinfo5
sudo apt install openjdk-11-jdk
sudo apt-get install liblz4-tool
pip3 install dataclasses

**说明：** 
以上安装命令适用于Ubuntu20.04，其他版本请根据安装包名称采用对应的安装命令。

**1、编译**

在Linux环境进行如下操作:

1） 进入源码根目录，执行如下命令进行版本编译。

```
./build.sh --product-name purple_pi_oh –ccache --no-prebuilt-sdk
```

2） 检查编译结果。编译完成后，log中显示如下：

```
[OHOS INFO] 
[OHOS INFO] 
[OHOS INFO] purple_pi_oh build success
[OHOS INFO] cost time: 0:16:27
=====build  successful=====
```

编译所生成的文件都归档在out/purple_pi_oh/目录下，结果镜像输出在
out/purple_pi_oh/packages/phone/images/ 目录下。

3） 编译源码完成，请进行镜像烧录。

**2、烧录工具**

烧写工具下载及使用参考官网对应文档。





