#include <stdio.h>
#include <filesystem>
#include <boost/filesystem.hpp>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

namespace fs = boost::filesystem;


std::string getFileTypeStr( struct stat stbuf )
{
    if (S_ISDIR(stbuf.st_mode)) return "Directory";
    else if (S_ISLNK(stbuf.st_mode)) return "Symbolic Link";
    else if (S_ISREG(stbuf.st_mode)) return "Regular File";
    else if (S_ISBLK(stbuf.st_mode)) return "Block Device";
    else if (S_ISCHR(stbuf.st_mode)) return "Character Device";
    else if (S_ISFIFO(stbuf.st_mode)) return "Fifo File";
    else if (S_ISSOCK(stbuf.st_mode)) return "Socket File";

    return "Unknown File";
}

int main(int argc, char * argv[])
{

    if (argc != 6)
    {
        printf("Usage: %s [cur_subxid_start] [cur_subxid_max] [new_subxid_start] [new_subxid_max] [rootfspath]\n", argv[0]);
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
            printf("%d\n", fchownat(AT_FDCWD, p.path().c_str(), newuid, newgid, AT_SYMLINK_NOFOLLOW));
        }
        else
        {
            fprintf(stderr,"WARN: File %s is not on the requested SUBUID space\n",  p.path().c_str());
        }
    }
    return 0;
}
