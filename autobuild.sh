set -x

dir=$(pwd)
rm -rf $dir/build/*

cd $dir/build && 
    cmake .. &&
    make
