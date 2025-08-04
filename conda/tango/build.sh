#!/bin/bash
cd tango/
cmake -Bbuild -H. -GNinja -DCMAKE_INSTALL_PREFIX=$PREFIX -DCMAKE_FIND_ROOT_PATH=$PREFIX -DPython3_FIND_STRATEGY=LOCATION
cmake --build build --target install
