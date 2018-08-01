#!/bin/bash

PACKAGENAME=niergui
echo Building RPMs..
GITROOT=`git rev-parse --show-toplevel`
cd $GITROOT
VER=1.0
REL=`git rev-parse --short HEAD`git
REL=`git log --oneline|wc -l`.$REL
RPMTOPDIR=$GITROOT/build
echo "Ver: $VER, Release: $REL"


rm -rf $RPMTOPDIR
# Create tarball
mkdir -p $RPMTOPDIR
cd $RPMTOPDIR
sed -e "s/%{ver}/$VER/" -e "s/%{rel}/$REL/" $GITROOT/CMakeLists.txt -i
cmake ..
cpack
cp *.rpm $GITROOT

