 # LXC-REMAP

 Remap UID/GID in LXC containers with new subuid/subgid ranges.
 
Author: Aaron Mizrachi (unmanarc) <aaron@unmanarc.com>   
License: LGPLv3   

 ***

 ## Copying LXC Containers with different SUB-UID/GID

Copying containers is very useful. Specially if you want to have your own template. 

When you use `lxc-copy`, rootfs files are copied with the same uid-gid range, so, if you are using unprivileged LXC containers, you may want each container to have their own UID/GID address space.

This may improve your LXC security and reduce chances of pivoting, specially if somebody escaped one untrusted container.

 ## Build Pre-requisites

You will need to install (eg. for ubuntu 20.04):

```
apt install cmake libboost-filesystem1.71-dev build-essential
```

Or for ubuntu 18.04:

```
apt install cmake libboost-filesystem1.65-dev build-essential
```

You may use other versions of libboost, so just modify the 1.71/1.65 version for your OS provided one, and modify apt for yum if you are in fedora/centos

 ## How to build

Basically you have to execute this in our project dir:

```
cmake -DCMAKE_INSTALL_PREFIX=/ .
make install
```

 ## Usage:

```
# lxc-remap 
Usage: 
Manual Mode: lxc-remap [cur_subxid_start] [cur_subxid_max] [new_subxid_start] [new_subxid_max] [dir_path]
  Auto Mode: lxc-remap [lxc_dir_path] # < Requires lxc_dir_path/config

Author: Aaron Mizrachi <aaron@unmanarc.com>
License: LGPLv3
Version: 0.3a
```

The first thing you should do is to copy the container:

`lxc-copy -n basecontainer -N newcontainer`

And.. After copying a container, you should identify/edit the target subuid/subgid.

You can do this by modifiying `~lxcuser/.local/share/lxc/newcontainer/config` :

eg.

```
# IDMAP config...
lxc.idmap = u 0 300000 65536
lxc.idmap = g 0 300000 65536
```

now, you may repwn the files with the following command (**as root**):


```
# ./lxc-remap /home/lxcuser/.local/share/lxc/newcontainer
```


The Auto Mode will read the config/env values:

```
[+] Auto Config [rootfs]: /home/lxcuser/.local/share/lxc/newcontainer/rootfs
[+] Auto Config [(ug)id_transforms]: current(0->65536), new(300000->365536)
[+] Current LXC Path: /home/lxcuser/.local/share/lxc/newcontainer
[+] Parent LXC Path: /home/lxcuser/.local/share/lxc, (uid:1000, gid:1000)
 [-] Changing ownership of config [/home/lxcuser/.local/share/lxc/newcontainer/config] (to uid:1000,gid:1000) - 0
 [-] Changing ownership of LXC dir [/home/lxcuser/.local/share/lxc/newcontainer] (to uid:300000,gid:1000) - 0
  [-] Transforming Directory [/home/lxcuser/.local/share/lxc/newcontainer/rootfs], [uid:300000:gid:300000]->[uid:300000:gid:300000] - CHOWN(300000,300000): 0, CHMOD(40555): NOT REQ.
 [-] Transforming Directory [/home/lxcuser/.local/share/lxc/newcontainer/rootfs/run], [uid:300000:gid:300000]->[uid:300000:gid:300000] - CHOWN(300000,300000): 0, CHMOD(40755): NOT REQ.

 .
 .
 .
```

**don't forget to adapt the previous commands to your requirements!**


## Assumptions / Preconditions:
 
 - Both subuid/subgid range should start in the same number, so if, for some reason, the start uid/gid number mismatch, don't continue with this tool as is, modify it! ;-)
 - The user (eg. lxcuser) should have enough subuid/subgid space to handle the new UID/GID addresses. check this in `/etc/subuid` and `/etc/subgid`

 ## Compatibility

 For extending the compatibility to older OS's (like RHEL7), I decided to use C++11 with boost libraries instead of using C++17 wich also have the filesystem required functions.
