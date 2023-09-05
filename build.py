# -*- coding: utf-8 -*-
# !/usr/bin/python

import os
import platform
import shutil

source_path = os.path.dirname(os.path.abspath(__file__))
build_dir = "build"
build_path = source_path + '/' + build_dir

def build_firmware():
    if not os.path.exists(build_dir):
        os.makedirs(build_dir)

    os.chdir(build_path)

    if platform.system() == 'Darwin':
        os.system(
            'cmake -G "Unix Makefiles" .. -DLIB_RUST_C=ON -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++')
    else:
        os.system('cmake -G "Unix Makefiles" .. -DLIB_RUST_C=ON')
    os.system('make -j')

if __name__ == '__main__':
    shutil.rmtree(build_path, ignore_errors=True)
    build_firmware()
