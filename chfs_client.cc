// chfs client.  implements FS operations using extent server
#include "chfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

chfs_client::chfs_client(std::string extent_dst)
{
    ec = new extent_client(extent_dst);
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
}

chfs_client::inum
chfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
chfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool
chfs_client::isfile(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    } 
    printf("isfile: %lld is not a file\n", inum);
    return false;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */

bool
chfs_client::isdir(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_DIR) {
        printf("isfile: %lld is a dir\n", inum);
        return true;
    } 
    printf("isfile: %lld is not a dir\n", inum);
    return false;
}

bool
chfs_client::issymlink(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_SYMLINK) {
        printf("isfile: %lld is a symlink\n", inum);
        return true;
    } 
    printf("isfile: %lld is not a symlink\n", inum);
    return false;
}

int
chfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int
chfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
int
chfs_client::setattr(inum ino, size_t size)
{
    int r = OK;

    /*
     * your code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */
    
    std::string buf;
    ec->get(ino, buf);  // read the whole file

    buf.resize(size);

    ec->put(ino, buf);

    return r;
}

int
chfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    
    // check if file has existed
    bool found = false;
    inum tmp;  // not necessary
    lookup(parent, name, found, tmp);
    if (found) {
        // has existed
        return EXIST;
    }

    // pick an inum
    ec->create(extent_protocol::T_FILE, ino_out);

    // add an entry into parent
    std::string buf;
    ec->get(parent, buf);
    buf.append(std::string(name) + ":" + filename(ino_out) + "/");
    ec->put(parent, buf);
    return r;
}

int
chfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

    // check if dir name has existed
    bool found = false;
    inum tmp;  // not necessary
    lookup(parent, name, found, tmp);
    if (found) {
        // has existed
        return EXIST;
    }

    // pick an inum
    ec->create(extent_protocol::T_DIR, ino_out);

    // add an entry into parent
    std::string buf;
    ec->get(parent, buf);
    buf.append(std::string(name) + ":" + filename(ino_out) + "/");
    ec->put(parent, buf);
    return r;
}

int
chfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
    std::list<dirent> list;
    readdir(parent, list);

    if (list.empty()) {
        found = false;
        return r;
    }

    for (std::list<dirent>::iterator it = list.begin(); it != list.end(); it++) {
        if (it->name.compare(name) == 0) {
            // exist
            found = true;
            ino_out = it->inum;
            return r;
        }
    }
    found = false;
    return r;
}

int
chfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;

    /*
     * your code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
    // my format of dir content: "name:inum/name:inum/name:inum/"

    // get directory content
    std::string buf;
    ec->get(dir, buf);

    // traverse directory content
    
    int name_start = 0;
    int name_end = buf.find(':');
    while (name_end != std::string::npos) {
        std::string name = buf.substr(name_start, name_end - name_start);
        int inum_start = name_end + 1;
        int inum_end = buf.find('/', inum_start);
        std::string inum = buf.substr(inum_start, inum_end - inum_start);
            
        struct dirent entry;
        entry.name = name;
        entry.inum = n2i(inum);

        list.push_back(entry);
        
        name_start = inum_end + 1;
        name_end = buf.find(':', name_start);
    }

    return r;
}

int
chfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;

    /*
     * your code goes here.
     * note: read using ec->get().
     */
    
    std::string buf;
    ec->get(ino, buf);  // read the whole file

    // off > file size
    if (off > buf.size()) {
        data = "";
        return r;
    }

    // off + read size > file size
    if (off + size > buf.size()) {
        data = buf.substr(off);
        return r;
    }

    // normal
    data = buf.substr(off, size);
    return r;
}

int
chfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;

    /*
     * your code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
    
    std::string buf;
    ec->get(ino, buf);
    
    // off + write size <= file size
    if (off + size <= buf.size()) {
        for (int i = off; i < off + size; i++) {
            buf[i] = data[i - off];
        }
        bytes_written = size;
        ec->put(ino, buf);
        return r;
    }

    // off + write size > file size
    buf.resize(off + size);
    for (int i = off; i < off + size; i++) {
        buf[i] = data[i - off];
    }
    bytes_written = size;
    ec->put(ino, buf);
    return r;
}

int chfs_client::unlink(inum parent,const char *name)
{
    int r = OK;

    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    
    bool found = false;  // not necessary
    inum inum;
    lookup(parent, name, found, inum);

    ec->remove(inum);

    // update parent directory content
    std::string buf;
    ec->get(parent, buf);
    int erase_start = buf.find(name);
    int erase_after = buf.find('/', erase_start);
    buf.erase(erase_start, erase_after - erase_start + 1);
    ec->put(parent, buf);

    return r;
}

int chfs_client::symlink(inum parent, const char *name, const char *link, inum &ino_out)
{
    int r = OK;

    // check if symlink name has existed
    bool found = false;
    inum tmp;  // not necessary
    lookup(parent, name, found, tmp);
    if (found) {
        // has existed
        return EXIST;
    }

    // pick an inum and init the symlink
    ec->create(extent_protocol::T_SYMLINK, ino_out);
    ec->put(ino_out, std::string(link));

    // add an entry into parent
    std::string buf;
    ec->get(parent, buf);
    buf.append(std::string(name) + ":" + filename(ino_out) + "/");
    ec->put(parent, buf);
    return r;
}

int chfs_client::readlink(inum ino, std::string &data)
{
    int r = OK;
    
    std::string buf;
    ec->get(ino, buf);

    data = buf;
    
    return r;
}