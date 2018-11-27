
# wav_play

播放wav文件的代码，可用于C或C++

#### 项目介绍
1. 分支master： 用于播放wav文件，默认wav参数为 16 bit ，1 channels，16K rate 程序基于alsa 1.1.3 aplay修改而来

2. 分支x1800： 由于君正x1800 alsa底层驱动的不完善性，导致播放存在一些问题（只能播放44.1KHZ 双声道），此分支代码用于君正x1800播放wav文件，默认wav参数为 16 bit ，2 channels，44.1K rate 程序基于alsa 1.1.2 aplay修改而来

3. 以上两个版本均可以作为其他平台的通用版本，x1800分支还可以消除播放最后的“嗒嗒”声

#### 软件架构
软件架构说明

#### 安装教程

1. 需要安装asound的库 sudo apt-get install libasound2-dev

#### 使用说明

1. 参考头文件playback.h中的注释说明
