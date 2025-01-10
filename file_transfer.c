#include "utils.h"
#include "file_transfer.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define BUFFER_SIZE 1

int new_file_transfer(char * path, struct dirent * entry, struct file_transfer * ft){
    if(entry){
        ft->mode = (entry->d_type == DT_REG) ? TR_FILE : TR_DIR;
        sprintf(ft->path, "%s/%s", path, entry->d_name);

        struct stat sb;
        int r = stat(ft->path,&sb);
        v_err(r, "stat err", 1);
        ft->size = sb.st_size;
    }
    else{
        ft->mode = TR_DIR;
        sprintf(ft->path, "%s", path);
        ft->size = -1;
    }
}

/*Transmit a file*/
int transmit_file(int transmit_fd, char * path, struct dirent * entry){
    struct file_transfer ft;
    new_file_transfer(path, entry, &ft);
    printf("sending %s\n",ft.path);
    write(transmit_fd, &ft, sizeof(struct file_transfer));
    
    if(ft.mode == TR_FILE){
        printf("sending file contents...\n");
        int fd = open(ft.path, O_RDONLY, 0);
        int bytes;
        char buffer[BUFFER_SIZE];
        printf("%d\n",sizeof(buffer));
        memset(buffer,0,sizeof(buffer));
        while(bytes = read(fd, buffer, sizeof(buffer))){
            v_err(bytes, "read err", 1);
            usleep(1000);
            write(transmit_fd, buffer, bytes);
            printf("sent file contents (%d): %s\n", bytes, buffer);
            memset(buffer,0,sizeof(buffer));
        }
        close(fd);
        // write(transmit_fd, "", 0);
    }
    return 0;
}

/*Recv a file*/
int recv_file(int recv_fd, struct file_transfer * ft){
    if (ft->mode == TR_FILE){
        printf("creating FILE: %s\n", ft->path);
        int fd = open(ft->path, O_CREAT | O_WRONLY, 0644);
        v_err(fd, "CREAT file failed", 0);
        
        int bytes_to_read = ft->size;
        printf("cpying %d bytes...\n", bytes_to_read);
        while (bytes_to_read > 0){
            // get file contents
            char buffer[BUFFER_SIZE];
            memset(buffer, 0, sizeof(buffer));
            int bytes;
            while( (bytes = read(recv_fd, &buffer, sizeof(buffer))) && bytes_to_read>0){
                write(fd, buffer, bytes);
                printf("recieved buffer with contents:\n    %s\n", buffer);
                memset(buffer, 0, sizeof(buffer));
                bytes_to_read -= bytes;
                printf("recived %d bytes, %d left\n", bytes, bytes_to_read);
            }
        }
        close(fd);
    }
    else if (ft->mode == TR_DIR){
        printf("creating DIR: %s\n", ft->path);
        int r = mkdir(ft->path,0744);
        v_err(r, "mkdir failed", 0);
    }
    else if(ft->mode == TR_END){
        printf("exit!!!!\n");
    }
}

/*tree transmit goes through the file system at dir path
and for each file/dir transmits the name to the transmit_fd.
This serves to provide the info to build the correct file system.
File contents will also be sent with this fxn.*/
int tree_transmit(char * path, int transmit_fd){
    DIR * d;
    d = opendir( path );
    v_err(d==NULL?-1:0, "could not open dir", 1);

    struct dirent * entry;
    struct stat * stat_buffer = malloc(sizeof(struct stat));

    while(entry = readdir(d)){
        usleep(1000);
        
        if(strcmp(entry -> d_name, ".") && strcmp(entry -> d_name, "..")){
            transmit_file(transmit_fd, path, entry);
            
            if (entry->d_type == DT_DIR ){
                char new_path[(strlen(path) + 2 + strlen(entry->d_name))*sizeof(char)];
                sprintf(new_path, "%s/%s", path, entry->d_name);
                tree_transmit(new_path, transmit_fd);
            }
        }
    }
        
    closedir(d);
}
