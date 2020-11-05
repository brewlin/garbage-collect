#!/bin/bash
assert(){
    input="$1"
    cd "$input"
    make
    ./gc
    if [  "$?" != 0 ]; then
        echo "\033[31m test $input failed code:$?\033[0m"
        exit 1
    fi
    cd ..
}
read_dir(){
    for file in `ls $1`
    do
     if [ -d $1"/"$file ] ; then
        assert $1"/"$file
        echo "\033[32m [compile] $file pass! \033[0m \n"
     fi
    done
}
read_dir ./
echo
echo "\033[32m all passing..... \033[0m"
