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

You may use other versions of libboost, so just modify the 1.71 version for your OS provided one, and modify apt for yum if you are in fedora/centos

 ## How to build

Basically you have to execute this in our project dir:

```
cmake -DCMAKE_INSTALL_PREFIX=/ .
make install
```

 ## Usage:

```
$ ./lxc-remap 
Usage: ./lxc-remap [cur_subxid_start] [cur_subxid_max] [new_subxid_start] [new_subxid_max] [rootfspath]
```

The first thing you should do is to copy the container:

`lxc-copy -n basecontainer -N newcontainer`

And.. After copying a container, you should identify the source subuid/subgid range and the target subuid/subgid range.

You can do this by looking in `~lxcuser/.local/share/lxc/basecontainer/config` :

eg.
```
# IDMAP config...
lxc.idmap = u 0 200000 65536
lxc.idmap = g 0 200000 65536
```

now, you may go to `~lxcuser/.local/share/lxc/newcontainer/config` and replace with the desired values:

eg.

```
# IDMAP config...
lxc.idmap = u 0 300000 65536
lxc.idmap = g 0 300000 65536
```

now, you may repwn the files with the following command (**as root**):


```
# ./lxc-remap 200000 265535 300000 365535 ~lxcuser/.local/share/lxc/newcontainer/rootfs
# chown 300000:300000 ~lxcuser/.local/share/lxc/newcontainer
```

**don't forget to adapt the previous commands to your requirements!**


## Assumptions / Preconditions:
 
 - Both subuid/subgid range should start in the same number, so if, for some reason, the start uid/gid number mismatch, don't continue with this tool as is, modify it! ;-)
 - The user (eg. lxcuser) should have enough subuid/subgid space to handle the new UID/GID addresses. check this in `/etc/subuid` and `/etc/subgid`

 ## Compatibility

 For extending the compatibility to older OS's (like RHEL7), I decided to use C++11 with boost libraries instead of using C++17 wich also have the filesystem required functions.
