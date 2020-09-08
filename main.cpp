/*
 * lxc-remap : Rewrite LXC SUB UID/GID Maps.
 * Copyright (C) 2020 Aaron Mizrachi <aaron@unmanarc.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation Version 3
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <boost/filesystem.hpp>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

namespace fs = boost::filesystem;


std::string getFileTypeStr( struct stat info )
{
    if (S_ISDIR(info.st_mode)) return "Directory";
    else if (S_ISLNK(info.st_mode)) return "Symbolic Link";
    else if (S_ISREG(info.st_mode)) return "Regular File";
    else if (S_ISBLK(info.st_mode)) return "Block Device";
    else if (S_ISCHR(info.st_mode)) return "Character Device";
    else if (S_ISFIFO(info.st_mode)) return "Fifo File";
    else if (S_ISSOCK(info.st_mode)) return "Socket File";

    return "Unknown File";
}

int toOctal(int d)
{
    int oct=0, i=1;

    while (d)
    {
        oct+=(d%8)*i;
        i*=10;
        d/=8;
    }

    return oct;
}

int main(int argc, char * argv[])
{

    if (argc != 6)
    {
        printf("Usage: %s [cur_subxid_start] [cur_subxid_max] [new_subxid_start] [new_subxid_max] [rootfs_path]\n", argv[0]);
        printf("Author: Aaron Mizrachi <aaron@unmanarc.com>\n");
        printf("License: LGPLv3\n");
        printf("Version: 0.2a\n");
        return -1;
    }

    unsigned int cur_space_start = strtoul(argv[1],nullptr,10);
    unsigned int cur_space_max = strtoul(argv[2],nullptr,10);
    unsigned int new_space_start = strtoul(argv[3],nullptr,10);
    unsigned int new_space_max = strtoul(argv[4],nullptr,10);

    int64_t diff = new_space_start-cur_space_start;

    printf("Ranges: current %u->%u, new %u->%u\n", cur_space_start,
           cur_space_max,
           new_space_start,
           new_space_max);

    if (getuid()!=0)
    {
        printf("ERR: This program should be executed with UID=0 (root). Aborting...\n");
        return -5;
    }
    if (access(argv[5],W_OK))
    {
        printf("ERR: Rootfs path is not a valid writteable location. Aborting...\n");
        return -4;
    }

    printf("Changing owner of base dir [%s] - %d\n",argv[5], fchownat(AT_FDCWD,argv[5], new_space_start, new_space_start, AT_SYMLINK_NOFOLLOW));

    for(auto& p: fs::recursive_directory_iterator(argv[5]))
    {
        struct stat info;
        if (fstatat(AT_FDCWD,p.path().c_str(), &info,AT_SYMLINK_NOFOLLOW))
        {
            printf("ERR: File %s is not accessible. Aborting...\n", p.path().c_str());
            return -3;
        }

        if (        (info.st_uid >= cur_space_start && info.st_uid<=cur_space_max )
                    &&     (info.st_gid >= cur_space_start && info.st_gid<=cur_space_max ) )
        {
            uint32_t newuid = info.st_uid+diff;
            uint32_t newgid = info.st_gid+diff;

            printf("Transforming %s [%s], [uid:%u:gid:%u]->[uid:%u:gid:%u] - ", getFileTypeStr(info).c_str(), p.path().c_str(), info.st_uid, info.st_gid, newuid, newgid);
            if (newuid > new_space_max || newgid > new_space_max)
            {
                fprintf(stderr,"\nERR: File exceed the %u limit, aborting!\n", new_space_max);
                return -2;
            }

            printf("CHOWN(%d,%d): %d, ",newuid,newgid, fchownat(AT_FDCWD, p.path().c_str(), newuid, newgid, AT_SYMLINK_NOFOLLOW));

            // Restore previous mode on non-links with special attribs... (chown sometimes removed the setuid).
            if (!S_ISLNK(info.st_mode) && (S_ISUID&info.st_mode || S_ISGID&info.st_mode || S_ISVTX&info.st_mode ))
                printf("CHMOD(%d): %d\n",toOctal(info.st_mode), fchmodat(AT_FDCWD, p.path().c_str(), info.st_mode, 0));
            else
                printf("CHMOD(%d): NOT REQ.\n",toOctal(info.st_mode));
        }
        else
        {
            fprintf(stderr,"WARN: File %s is not on the requested SUBUID space\n",  p.path().c_str());
        }
    }
    return 0;
}
