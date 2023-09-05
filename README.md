# keystone3-bootloader
This is the bootloader repo for Keystone 3 devices.

[![Build status](https://badge.buildkite.com/cd4b59812968ce7984531a54e7fac3d2506277a7a44e347d30.svg)](https://buildkite.com/keystonehq/project-pillar-bootloader)

### How to use `buildkite` to build bootloader
1. Open Buildkite：https://buildkite.com/keystonehq/project-pillar-bootloader , please find the username and password in 1P.
2. Click the `New Build` Green Button to create a new build, set the branch to "master".
3. After the CI done, all the firmware will be listed on the S3 bucket `keystone-g3-bootloader`.
4. You will find the s3 username/password in 1P(search "AWS S3 Access").
