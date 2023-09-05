#!/bin/bash

set -e

cd $(dirname $0)/..

source ci/env

image=623147552995.dkr.ecr.eu-central-1.amazonaws.com/$app_name:latest

docker run -v $(pwd):/project-pillar-bootloader \
-e AWS_DEFAULT_REGION=$AWS_DEFAULT_REGION \
-e AWS_ACCESS_KEY_ID=$AWS_ACCESS_KEY_ID \
-e AWS_SECRET_ACCESS_KEY=$AWS_SECRET_ACCESS_KEY \
-e AWS_SESSION_TOKEN=$AWS_SESSION_TOKEN \
$image python3 build.py

echo "uploading artifacts"
aws s3 cp $(pwd)/build/mh1903_boot.bin s3://keystone-g3-bootloader
aws s3 cp $(pwd)/build/mh1903_boot.elf s3://keystone-g3-bootloader
aws s3 cp $(pwd)/build/mh1903_boot.hex s3://keystone-g3-bootloader
aws s3 cp $(pwd)/build/mh1903_boot.map s3://keystone-g3-bootloader