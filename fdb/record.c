#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "fdb.h"

int record_create(const char *path, uint32_t record_count)
{
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd == -1)
    {
        perror("open failed");
        return -1;
    }
    fdb_header_t hdr;
    hdr.magic = FDB_MAGIC;
    hdr.version = FDB_VERSION;
    hdr.record_size = FDB_RECORD_SIZE;
    hdr.record_count = record_count;
    hdr.free_list_head = -1;
    if (write(fd,&hdr,sizeof(hdr))==-1)
    {
        perror("write failed");
        return -1;
    }
    fdb_record_t rec;
    memset(&rec,0,sizeof(rec));
    for (int i=0;i<record_count;i++)
    {
        if (write(fd,&rec,sizeof(rec))==-1)
        {
            perror("write failed");
            return -1;
        }
    }
    return fd;
}

int record_read_header(int fd, fdb_header_t *hdr)
{
    lseek(fd,0,SEEK_SET);
    ssize_t n = read(fd,hdr,sizeof(fdb_header_t));
    if (n==-1)
    {
        perror("read failed");
        return -1;
    }
    if (n!=sizeof(fdb_header_t))
    {
        fprintf(stderr,"read header : got %zd bytes, expected %zu\n",n,sizeof(fdb_header_t));
        return -1;
    }
    return 0;
}

int record_write_header(int fd, const fdb_header_t *hdr)
{
    lseek(fd,0,SEEK_SET);
    if (write(fd,hdr,sizeof(fdb_header_t))==-1)
    {
        perror("write failed");
        return -1;
    }
    return 0;
}

int record_read(int fd, int rec_no, fdb_record_t *rec)
{
    lseek(fd,FDB_HEADER_SIZE+rec_no*FDB_RECORD_SIZE,SEEK_SET);
    ssize_t n = read(fd,rec,sizeof(fdb_record_t));
    if (n==-1)
    {
        perror("read failed");
        return -1;
    }
    if (n!=sizeof(fdb_record_t))
    {
        fprintf(stderr,"read record : got %zd bytes, expected %zu\n",n,sizeof(fdb_record_t));
        return -1;
    }
    return 0;
}


int record_write(int fd, int rec_no, const fdb_record_t *rec)
{
    lseek(fd,FDB_HEADER_SIZE+rec_no*FDB_RECORD_SIZE,SEEK_SET);
    if (write(fd,rec,sizeof(fdb_record_t))==-1)
    {
        perror("write failed");
        return -1;
    }
    return 0;
}

int record_atomic_write(const char *path, int fd,int rec_no, const fdb_record_t *rec,const fdb_header_t *hdr)
{
    char tmp_path[256];
    snprintf(tmp_path,sizeof(tmp_path),"%s.tmp",path);
    int fd2 = open(tmp_path,O_RDWR | O_CREAT | O_TRUNC,0644);
    int rd  = 1;
    int i   = 0;
    fdb_record_t rec2;
    if (fd2==-1)
    {
        perror("open failed");
        return -1;
    }
    lseek(fd2,0,SEEK_SET);
    if (write(fd2,hdr,sizeof(fdb_header_t))==-1)
    {
        perror("write failed");
        return -1;
    }
    
    while (i<hdr->record_count)
    {
        if (i==rec_no)
        {
            lseek(fd2,FDB_HEADER_SIZE+i*FDB_RECORD_SIZE,SEEK_SET);
            if (write(fd2,rec,sizeof(fdb_record_t))==-1)
            {
                perror("write failed");
                return -1;
            }
            i++;
            continue;
        }
        lseek(fd,FDB_HEADER_SIZE+i*FDB_RECORD_SIZE,SEEK_SET);
        rd = read(fd,&rec2,sizeof(fdb_record_t));
        if (rd == -1)
        {
            perror("read failed");
            return -1;
        }
        if (rd != sizeof(fdb_record_t))
        {
            fprintf(stderr,"read record : got %zd bytes, expected %zu\n",rd,sizeof(fdb_record_t));
            return -1;
        }
        lseek(fd2,FDB_HEADER_SIZE+i*FDB_RECORD_SIZE,SEEK_SET);
        if (write(fd2,&rec2,sizeof(fdb_record_t))==-1)
        {
            perror("write failed");
            return -1;
        }
        i++;
    }
    if (fsync(fd2) == -1)
    {
        perror("fsync failed");
        return -1;
    }
    if (rename(tmp_path,path)==-1)
    {
        perror("rename failed");
        return -1;
    }
    close(fd2);
    return 0;
    
}
