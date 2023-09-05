# project-pillar-bootloader
This is the bootloader project of PILLAR.

[![Build status](https://badge.buildkite.com/cd4b59812968ce7984531a54e7fac3d2506277a7a44e347d30.svg)](https://buildkite.com/keystonehq/project-pillar-bootloader)

## mdk
使用MDK-ARM 5.37编译，需安装keil4 legacy支持包。(https://www2.keil.com/mdk5/legacy/) ，从5.36提取了旧的 ARM Compiler V5.06 update 7(build 960).

## gcc
使用arm-none-eabi-编译，首次编译需要在gcc目录下手动新建obj和bin目录。

## sdk
/mh1903_lib 目录为mh1903的SDK。

### How to use `buildkite` to build bootloader
1. Open Buildkite：https://buildkite.com/keystonehq/project-pillar-bootloader , please find the username and password in 1P.
2. Click the `New Build` Green Button to create a new build, set the branch to "master".
3. After the CI done, all the firmware will be listed on the S3 bucket `keystone-g3-bootloader`.
4. You will find the s3 username/password in 1P(search "AWS S3 Access").
