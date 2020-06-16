#! /bin/bash

# get a compiler that allows for using modern-ish C++ (>= 11) on a distro that doesn't normally support it
# before you ask: yes, the binaries will work on CentOS 6 even without devtoolset (they somehow partially link C++
# things statically while using others from the system...)
# so, basically, it's magic!
source /opt/rh/devtoolset-*/enable

exec "$@"
