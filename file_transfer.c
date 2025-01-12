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

#define BUFFER_SIZE 512

int send_full_directory_contents(int transmit_fd, char * path){
    //send the root of the file sys
  transmit_file(transmit_fd, path,NULL);

  //send the file system
  tree_transmit(path, transmit_fd);

  //end connection
  send_end(transmit_fd);
  printf("sending exit. closing connection...\n");
}

int send_end(int new_socket){
  struct file_transfer ft;
  new_file_transfer("",NULL,&ft);
  ft.size = -1;
  ft.mode = TR_END;
  write(new_socket, &ft, sizeof(struct file_transfer));
}

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


int send_file_contents(int transmit_fd, struct file_transfer * ft){
    //open the target file
    int fd = open(ft->path, O_RDONLY, 0);

    //bytes read from fd
    int bytes;

    //sending buffer is zeroed
    char buffer[BUFFER_SIZE];
    memset(buffer,0,sizeof(buffer));

    //read from file
    while((bytes = read(fd, buffer, sizeof(buffer)))){
        v_err(bytes, "read err", 1);

        //write the server the bytes
        write(transmit_fd, buffer, bytes);

        printf("    sent %d bytes of %d\n", bytes, ft->size);
        
        //zero the buffer
        memset(buffer,0,sizeof(buffer));
    }
    close(fd);
}

/*Transmit a file*/
int transmit_file(int transmit_fd, char * path, struct dirent * entry){

    //build the ft struct for this specific path
    struct file_transfer ft;
    new_file_transfer(path, entry, &ft);
   
    //write transmit_fd instruction struct
    write(transmit_fd, &ft, sizeof(struct file_transfer));
    
    //if it is a file
    if(ft.mode == TR_FILE){
        printf("sending file contents for %s...\n", ft.path);
        send_file_contents(transmit_fd, &ft);
    }

    if(ft.mode == TR_DIR){
        printf("sent dir: %s\n", ft.path);
    }

    return 0;
}


int recv_file_contents(int recv_fd, struct file_transfer * ft){
    int fd = open(ft->path, O_CREAT | O_WRONLY, 0644);
    v_err(fd, "CREAT file failed", 0);
    
    int bytes_left = ft->size;
    
    // init empty buffer and set to 0
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    //init byte return from read
    int bytes;

    //if buffer > bytes left, only read bytes left bytes
    int bytes_to_read = bytes_left > BUFFER_SIZE ? BUFFER_SIZE : bytes_left;

    //read from server stream
    while( (bytes = read(recv_fd, &buffer, bytes_to_read)) && bytes_left>0){

        v_err( bytes, "write err", 0);

        printf("    recieved %d bytes of %d \n", bytes, ft->size);
        
        //write to file
        write(fd, buffer, bytes);

        memset(buffer, 0, sizeof(buffer));
    
        //now we read bytes...
        bytes_left -= bytes;
        //how many to read next time?
        bytes_to_read = bytes_left > BUFFER_SIZE ? BUFFER_SIZE : bytes_left;
    }
    
    //close our new file
    close(fd);
}

/*Recv a file*/
int recv_file(int recv_fd, struct file_transfer * ft){
    if (ft->mode == TR_FILE){
        printf("creating file: %s\n", ft->path);
        recv_file_contents(recv_fd, ft);
    }
    else if (ft->mode == TR_DIR){
        printf("creating directory: %s\n", ft->path);
        int r = mkdir(ft->path,0744);
        v_err(r, "mkdir failed", 0);
    }

    return 0;
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
        // usleep(1000);
        
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
