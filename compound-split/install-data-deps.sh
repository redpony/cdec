#!/bin/sh
set -e

data_version=csplit-data-01.tar.gz

ROOTDIR=`dirname $0`
cd $ROOTDIR

curl -f http://demo.clab.cs.cmu.edu/cdec/$data_version -o $data_version

tar xzf $data_version
rm -f $data_version

