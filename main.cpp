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

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <boost/algorithm/string.hpp>

namespace fs = boost::filesystem;
using namespace std;


string getFileTypeStr( struct stat info )
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

int chownDir( const string & dirPath,
              unsigned int cur_space_start,
              unsigned int cur_space_max,
              unsigned int new_space_max,
              int64_t diff
              )
{
    struct stat info;
    if (fstatat(AT_FDCWD,dirPath.c_str(), &info,AT_SYMLINK_NOFOLLOW))
    {
        printf(" [-] ERR: File %s is not accessible. Aborting...\n", dirPath.c_str());
        return -3;
    }

    if (        (info.st_uid >= cur_space_start && info.st_uid<=cur_space_max )
                &&     (info.st_gid >= cur_space_start && info.st_gid<=cur_space_max ) )
    {
        uint32_t newuid = info.st_uid+diff;
        uint32_t newgid = info.st_gid+diff;

        printf(" [-] Transforming %s [%s], [uid:%u:gid:%u]->[uid:%u:gid:%u] - ", getFileTypeStr(info).c_str(), dirPath.c_str(), info.st_uid, info.st_gid, newuid, newgid);
        if (newuid > new_space_max || newgid > new_space_max)
        {
            fprintf(stderr,"\nERR: File exceed the %u limit, aborting!\n", new_space_max);
            return -2;
        }

        printf("CHOWN(%d,%d): %d, ",newuid,newgid, fchownat(AT_FDCWD, dirPath.c_str(), newuid, newgid, AT_SYMLINK_NOFOLLOW));

        // Restore previous mode on non-links with special attribs... (chown sometimes removed the setuid).
        if (!S_ISLNK(info.st_mode) && (S_ISUID&info.st_mode || S_ISGID&info.st_mode || S_ISVTX&info.st_mode ))
            printf("CHMOD(%d): %d\n",toOctal(info.st_mode), fchmodat(AT_FDCWD, dirPath.c_str(), info.st_mode, 0));
        else
            printf("CHMOD(%d): NOT REQ.\n",toOctal(info.st_mode));
    }
    else
    {
        fprintf(stderr,"WARN: File %s is not on the requested SUBUID space\n",  dirPath.c_str());
    }
    return 0;
}

int main(int argc, char * argv[])
{
    if (argc != 6 && argc!=2)
    {
        printf("Usage: \n"
               "Manual Mode: %s [cur_subxid_start] [cur_subxid_max] [new_subxid_start] [new_subxid_max] [dir_path]\n"
               "  Auto Mode: %s [lxc_dir_path] # < Requires lxc_dir_path/config\n"
               , argv[0], argv[0]);
        printf( "\n"
                "Author: Aaron Mizrachi <aaron@unmanarc.com>\n"
                "License: LGPLv3\n"
                "Version: 0.3a\n");
        return -1;
    }

    ///////////////////////////////
    // vars:
    string rootfsDir, lxcDir;
    uint32_t cur_space_start=0, cur_space_max=65536, new_space_start=0, new_space_max=65536;

    ///////////////////////////////
    // Root check:
    if (getuid()!=0)
    {
        fprintf(stderr,"ERR: This program should be executed with UID=0 (root). Aborting...\n");
        return -5;
    }

    ///////////////////////////////
    // Vars init:
    if (argc == 6)
    {
        rootfsDir = argv[5];
        cur_space_start = strtoul(argv[1],nullptr,10);
        cur_space_max = strtoul(argv[2],nullptr,10);
        new_space_start = strtoul(argv[3],nullptr,10);
        new_space_max = strtoul(argv[4],nullptr,10);

        printf("[+] Manual Config: current %u->%u, new %u->%u\n", cur_space_start,cur_space_max,new_space_start,new_space_max);
    }

    if (argc == 2)
    {
        lxcDir = argv[1];

        if (boost::ends_with(lxcDir,"/"))
        {
            fprintf(stderr,"ERR: don't use slash at the end of the dir name...  %s\n", lxcDir.c_str());
            return -9;
        }

        string configFile = lxcDir + "/config";
        if (access( configFile.c_str(), W_OK ))
        {
            fprintf(stderr,"ERR: %s not found or writteable. Aborting...\n", configFile.c_str());
            return -6;
        }

        uint32_t space_max = 65535;
        ifstream in(configFile);
        bool idMapOK = false;
        string tp;
        while (std::getline(in,tp))
        {
            vector<string> strs;
            boost::split(strs,tp,boost::is_any_of("="));

            if ( strs.size() == 2 )
            {
                string key   = boost::algorithm::to_lower_copy( boost::algorithm::trim_copy( strs[0] ) );
                string value = boost::algorithm::trim_copy( strs[1] );

                if (key == "lxc.rootfs.path")
                {
                    if (!boost::algorithm::istarts_with(value,"dir:"))
                    {
                        fprintf(stderr,"ERR: rootfs config not dir: %s\n", value.c_str());
                        return -8;
                    }
                    rootfsDir = value.substr(4);
                    printf("[+] Auto Config [rootfs]: %s\n", rootfsDir.c_str());
                }
                else if (key == "lxc.idmap")
                {
                    vector<string> strs_idmap;
                    boost::split(strs_idmap,value,boost::is_any_of(" "),boost::token_compress_on);

                    if (strs_idmap.size()<4)
                    {
                        printf("[+] Warning!!! idmap should have at least 4 parameters, current is: %s\n", value.c_str());
                    }
                    else if (!idMapOK)
                    {
                        new_space_start = stoul(strs_idmap[2]);
                        new_space_max = new_space_start+stoul(strs_idmap[3]);
                        space_max = stoul(strs_idmap[3]);
                        idMapOK = true;
                    }
                }
            }
        }
        in.close();

        if (!idMapOK)
        {
            printf("[+] Error!!! lxc.idmap not found in config\n");
            return -8;
        }

        struct stat info;
        if (fstatat(AT_FDCWD,rootfsDir.c_str(), &info,AT_SYMLINK_NOFOLLOW))
        {
            printf("ERR: Dir %s is not accessible. Aborting...\n", rootfsDir.c_str());
            return -3;
        }
        cur_space_start = info.st_uid;
        cur_space_max = info.st_uid+space_max;

        printf("[+] Auto Config [(ug)id_transforms]: current(%u->%u), new(%u->%u)\n", cur_space_start,cur_space_max,new_space_start,new_space_max);

        boost::filesystem::path p(lxcDir);
        struct stat info_parent;

        if (fstatat(AT_FDCWD,p.parent_path().c_str(), &info_parent,AT_SYMLINK_NOFOLLOW))
        {
            printf("ERR: Parent dir %s is not accessible. Aborting...\n", p.parent_path().c_str());
            return -3;
        }

        printf("[+] Current LXC Path: %s \n", p.c_str());
        printf("[+] Parent LXC Path: %s, (uid:%u, gid:%u)\n", p.parent_path().c_str(), info_parent.st_uid, info_parent.st_gid);

        printf(" [-] Changing ownership of config [%s] (to uid:%u,gid:%u) - %d\n", configFile.c_str(),  info_parent.st_uid, info_parent.st_gid,        fchownat(AT_FDCWD,configFile.c_str(), info_parent.st_uid, info_parent.st_gid, AT_SYMLINK_NOFOLLOW));
        printf(" [-] Changing ownership of LXC dir [%s] (to uid:%u,gid:%u) - %d\n", lxcDir.c_str(    ), new_space_start,    info_parent.st_gid,        fchownat(AT_FDCWD,lxcDir.c_str()    , new_space_start   , info_parent.st_gid, AT_SYMLINK_NOFOLLOW));
    }

    int64_t diff = new_space_start-cur_space_start;

    if (access(rootfsDir.c_str(),W_OK))
    {
        printf("ERR: Rootfs path is not a valid writteable location. Aborting...\n");
        return -4;
    }

    int i = chownDir( rootfsDir, cur_space_start, cur_space_max, new_space_max, diff);
    if (i) return i;

    for(auto& p: fs::recursive_directory_iterator(rootfsDir.c_str()))
    {
        i = chownDir( p.path().c_str(), cur_space_start, cur_space_max, new_space_max, diff);
        if (i) return i;
    }
    return 0;
}
