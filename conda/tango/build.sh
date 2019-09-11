#!/bin/bash
cd tango/
cmake -Bbuild -H. -DCMAKE_INSTALL_PREFIX=$PREFIX -DPYTHON_SITE_PACKAGES_DIR=$SP_DIR -DCMAKE_FIND_ROOT_PATH=$PREFIX
cmake --build build --target install
