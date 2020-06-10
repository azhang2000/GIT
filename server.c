#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>
#include<fcntl.h>
#include<openssl/md5.h>

#define MAXRCVLEN 500
#define PORTNUM 2343


struct node{
    char * project;
    pthread_mutex_t lock;
    struct node * next;
};

void check(int n){
    if (n==-1){
        perror("Error");
        exit(1);
    }
}

char * newString(int len){
    char * buffer = (char *)malloc(len * sizeof(char));
    memset(buffer, '\0', len);
    return buffer;
}

struct node * locks;
void lock(char * project){
    struct node * ptr = locks;
    while (ptr!=NULL){
        if (strcmp(ptr->project,project)==0)
        {
            pthread_mutex_lock(&ptr->lock);
            return;
        }
        ptr = ptr->next;
    }
    ptr = (struct node *)malloc(sizeof(struct node));
    ptr->project = newString(strlen(project)+1);
    strcpy(ptr->project, project);
    pthread_mutex_init(&ptr->lock,NULL);
    pthread_mutex_lock(&ptr->lock);
}

int unlock(char * project){
    struct node * ptr = locks;
    while (ptr!=NULL){
        if (strcmp(ptr->project,project)==0)
        {
            pthread_mutex_unlock(&ptr->lock);
            return 1;
        }
        ptr = ptr->next;
    }
    return 0;
}

int numOfD(int n){
    int count = 0;
    while (n != 0){
        n /= 10;
        count++;
    }
    return count;
}

int relen(int len){
    return ((len*2)-1);
}


char * resize(char * buffer, int len){
    char * temp = newString(len);
    memcpy(temp, buffer, len);
    free(buffer);
    len = relen(len);
    buffer = newString(len);
    strcat(buffer, temp);
    free(temp);
    return buffer;
}

char * getAbsolutePath( char * project, char * file){
    char * path = newString(strlen(project)+strlen(file)+4);
    strcat(path,"./");
    strcat(path,project);
    strcat(path,"/");
    strcat(path,file);
    return path;
}

char * getToken(char * buffer, int start){
    int i = strcspn(buffer+start, ":");
    char * token = newString(i+1);
    strncpy(token, buffer+start,i);
    return token;
}

char * readfile(int fd){
    int n, len = MAXRCVLEN + 1;
    char * buffer = newString(len);
    char * recvline = newString(len);

    while ((n=read(fd, recvline, MAXRCVLEN)) > 0){ 
        if ((n+strlen(buffer)) >= len){
            buffer = resize(buffer, len);
            len = relen(len);
        }
        strcat(buffer, recvline);
        memset(recvline, '\0', MAXRCVLEN+1);
    }
    free(recvline);
    return buffer;
}

char * readsocket(int fd){
    int status, i = 0; 
    int len = MAXRCVLEN + 1;
    char * buffer = newString(len);
    do {
        if (strlen(buffer)+1>=len){
	  buffer = resize(buffer,len);
            len = relen(len);
        }
        status = read(fd, buffer + i, 1);
        if (status <= 0) {
            break;
        }
        i++;
    } while (buffer[i-1]!='\0');
    
    return buffer;
}

int getCommitVersion(char * project){
    int commitversion = -1;
    struct dirent * dp;
    DIR * dir = opendir(project);
    if(dir!=NULL){
        readdir(dir);
        readdir(dir);
        while((dp=readdir(dir))!=NULL){ 
	        if(dp->d_type == DT_REG){ // its a file
                char * commitfilename = strstr(dp->d_name, ".Commit");
                if (commitfilename!=NULL){    
                    if(commitversion<=atoi(commitfilename+7)){
                        commitversion = atoi(commitfilename+7);
	                }
                }
	        }      
	    }
	}

	return commitversion;
}


char * getCurrentVersion(char * project){
    char * version = newString(10);
    struct dirent * dp;
    DIR * dir = opendir(project);
    if(dir!=NULL){
        readdir(dir);
        readdir(dir);
        while((dp=readdir(dir))!=NULL){
	        if(dp->d_type == DT_DIR){
	            if(atoi(version)<=atoi(dp->d_name)){
	                memset(version,'\0',10);
	                strcat(version,dp->d_name);
	            }
	        }      
	    }
	}

	return version;
}

char * getAbsoluteServerPath(char * project, char * file){
  char * path = newString(strlen(project)+strlen(file)+15);
  char * version = getCurrentVersion(project);
    strcat(path,"./");
    strcat(path,project);
    strcat(path,"/");
    strcat(path,version);
    strcat(path,"/");
    strcat(path,file);
    free(version);
    return path;
}

char * readManifest(char * project){
    char * manipath = getAbsoluteServerPath(project,".Manifest");
    int fd = open(manipath, O_RDONLY);
    check(fd);
    char * contents = readfile(fd);
    close(fd);
    return contents;
}

void receivefiles(char * protocol){
    char * numfiles = getToken(protocol,5);
    int fileno = 0;
    int i = 6 + strlen(numfiles);
    
    while(fileno<atoi(numfiles)){
        char * file = getToken(protocol,i);
        i = i + strlen(file)+1;
        
        printf("%s\n",file);

        char * fsize = getToken(protocol,i);
        i = i + strlen(fsize)+1;
        
        int filelen = atoi(fsize);
        char * fcontents = newString(filelen+1);
        strncpy(fcontents,protocol+i,filelen);
        
        i=i+filelen+1;
        printf("%s\n",fcontents);
        
        free(fcontents);
        free(fsize);
        free(file);
        fileno++;
    }

    free(numfiles);
}

void sendfiles(int mysocket, char * files,char * project){
    int linelen = 0;
    int start = 0;
    int len = MAXRCVLEN+1;
    int filenum=0;
    char * result = newString(len);
    do{
        linelen = strcspn(files+start, "\n");
        if(linelen>0){
            char * file = newString(linelen+1);
            strncpy(file, files+start,linelen);
            char * absolutePath = getAbsolutePath(project,file);
            char * absoluteServerPath = getAbsoluteServerPath(project,file);
            int fd = open(absoluteServerPath,O_RDONLY);
            check(fd);
            filenum++;
            char * filecontents = readfile(fd);
            char * size = newString(numOfD(strlen(filecontents))+1);
            sprintf(size,"%d",strlen(filecontents));
            if ((strlen(result)+strlen(filecontents) + strlen(file) + strlen(size) + 3) >= len){ // +3 for the : delimiter
                result = resize(result, len);
                len = relen(len);
            }
            strcat(result,absolutePath);
            strcat(result,":");
            strcat(result,size);
            strcat(result,":");
            strcat(result,filecontents);
            strcat(result,":");
            free(filecontents);
            free(file);
            free(absolutePath);
            free(absoluteServerPath);
            free(size);
            close(fd);
        }
        start+=linelen+1;
    } while(start<strlen(files)); 
    char * numfile = newString(numOfD(filenum)+1);
    sprintf(numfile,"%d",filenum);
    char * protocol = newString(7+strlen(result)+strlen(numfile));
    strcat(protocol,"send:");
    strcat(protocol,numfile);
    strcat(protocol,":");;
    strcat(protocol,result);
    //printf("%s\n",protocol);
    check(send(mysocket,protocol,strlen(protocol)+1,0));
    free(result);
    free(numfile);
    free(protocol);
}

void history(int consocket, char * project){
    struct stat st = {0};
    if (stat(project, &st) == -1) {
        check(send(consocket, "Failure", 8, 0));
        return;
    }
        check(send(consocket, "Success!", 9, 0));
  char * historypath = getAbsolutePath(project,".History");
  int fd = open(historypath,O_RDONLY);
  check(fd);
  char * history = readfile(fd);
  check(send(consocket,history,strlen(history)+1,0));
  free(history);
  free(historypath);
  close(fd);
}


void createDirectories(char * filepath){
    int len = strlen(filepath);
    int lastIndex = 0;
    int i = 0;
    for(i=0;i<len-1;i++){
        if(filepath[i]=='/'){
            lastIndex=i;
        }
    }
    char * directory = newString(lastIndex+2);
    strncpy(directory,filepath,lastIndex+1);
    struct stat st = {0};
    if (stat(directory, &st) == -1) {
        createDirectories(directory);
        mkdir(directory,00700);
    }
    free(directory);
    return;
}

char * createProject(char * project){
    struct stat st = {0};
    if (stat(project, &st) == -1) {
        mkdir(project, 00700);
	char * v = newString(strlen(project)+1+2);
	strcat(v,project);
	strcat(v,"/0");
	mkdir(v,00700);
        char * path = getAbsolutePath(project,"/0/.Manifest");
        int fd = open(path,O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
        check(fd);
        char * manifest = newString(strlen(project)+4);
        strcat(manifest,project);
        strcat(manifest,"\n0\n");
        write(fd,manifest,strlen(manifest));
        close(fd);
        return manifest;
    }
    else{
        return newString(1);
    }
}

void checkout(int consocket, char * project){
      struct stat st = {0};   
  if (stat(project, &st) == -1) {
        check(send(consocket, "failure", 8, 0));
        return;
    }
    int len = MAXRCVLEN+1;
    int linelen, start = 0;
    int line = 0;
    char * listoffiles = newString(len);
    char * manifest = readManifest(project);
    do{
        linelen = strcspn(manifest+start, "\n");
        if(linelen>0){
            if(line>1){
                int namelen = strcspn(manifest+start," ");
                char * file = newString(namelen+1);
                strncpy(file, manifest+start,namelen);            
                if(strlen(listoffiles)+strlen(file)+2 >= len){
                    listoffiles = resize(listoffiles,len);
                    len=relen(len);
                }
                strcat(listoffiles,file);
                strcat(listoffiles,"\n");
                free(file);
            }
            line++;
        }
        start+=linelen+1;
    }while(start<strlen(manifest));
    if(strlen(listoffiles)+10+2 >= len){
        listoffiles = resize(listoffiles,len);
        len=relen(len);
    }
    strcat(listoffiles,".Manifest");
    strcat(listoffiles,"\n");
    sendfiles(consocket,listoffiles,project);
    free(manifest);
    free(listoffiles);
}

void commit(int consocket, char * project){
    char * manifest = readManifest(project);
    printf("%s\n",manifest);
    check(send(consocket,manifest,strlen(manifest)+1,0));
    char * buffer = readsocket(consocket);
    //printf("%s\n",buffer);
    int commitIndex = getCommitVersion(project);
    commitIndex++;
    char * commitv = newString(8+numOfD(commitIndex));
    sprintf(commitv,".Commit%d",commitIndex);
    char * commitpath = getAbsolutePath(project, commitv);
    //printf("%s\n",commitpath);
    int fd = open(commitpath,O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
    check(fd);
    write(fd, buffer, strlen(buffer));
    free(manifest);
    free(buffer);
    free(commitv);
    free(commitpath);
    close(fd);
}

void copyDirectory(char * srcdir, char * destdir){
    char * command = newString(strlen(srcdir) + strlen(destdir)+8);
    strcat(command, "cp -R ");
    strcat(command, srcdir);
    strcat(command, " ");
    strcat(command, destdir);
    system(command);
    free(command);
}

void deleteDirectory(char * directory){
  char * command = newString(strlen(directory)+8);
  strcat(command,"rm -rf ");
  strcat(command,directory);
  system(command);
  free(command);
}

void destroy(int consocket, char * project){
    struct stat st = {0};
    if (stat(project, &st) == -1) {
        check(send(consocket, "Failure", 8, 0));
    }
    else{
        check(send(consocket, "Success!", 9, 0));
        deleteDirectory(project);
    }
}

void rollback(int consocket,char * project, char * version){
    struct stat st = {0}; 
    if (stat(project, &st) == -1 || atoi(version) > atoi(getCurrentVersion(project)) || version < 0) {
        check(send(consocket, "Failure", 8, 0));
        return;
    }
    check(send(consocket, "Success!", 9, 0));
        
    struct dirent * dp;
    DIR * dir = opendir(project);
    if(dir!=NULL){
        readdir(dir);
        readdir(dir);
        while((dp=readdir(dir))!=NULL){
	        if(dp->d_type == DT_DIR){
	            if(atoi(version)<atoi(dp->d_name)){
                    char * directory = newString(strlen(dp->d_name)+strlen(project)+3);	      
                    strcat(directory,project);
                    strcat(directory,"/");
                    strcat(directory,dp->d_name);
                    deleteDirectory(directory);
	            }
	        }
	    }
	}
}

int getNamelen(char * line){
    return strcspn(line, " ");
}

int getVersionlen(char * line){
    return strcspn(line+getNamelen(line)+1, " ");
}

int getHashlen(char * line){
    return strcspn(line+getNamelen(line)+getVersionlen(line)+2, "\n");
}

char * getFilename(char * line){
    int namelen = getNamelen(line);
    char * filename = newString(namelen+1);
    strncpy(filename,line, namelen);
    return filename;
} 

char * getVersion(char * line){
    int versionlen = getVersionlen(line);
    char * version = newString(versionlen+1);
    strncpy(version, line+getNamelen(line)+1, versionlen);
    return version;
}

char * getFileHash(char * line){
    int hashlen = getHashlen(line);
    char * hash = newString(hashlen+1);
    strncpy(hash, line+getNamelen(line)+getVersionlen(line)+2, hashlen);
    return hash;
}

char * getLine(char * file, int lineNum){
    int linelen, start = 0;
    int lineCounter = 0;
    
    char * line = newString(4);
    strcat(line, "EOF");
    do {
        linelen = strcspn(file+start, "\n");
        if (lineNum == lineCounter){
            if (linelen>0){
                free(line);
                line = newString(linelen+1);
                strncpy(line, file+start, linelen);
                return line;
            }
            else{
                free(line);
	            return newString(1);  
	        }
        }
        lineCounter++;
        start+=linelen+1;
    } while(start<strlen(file));
    return line;
}

char * resetLine(char * line, char * file, int lineNum){
    free(line);
    return getLine(file,lineNum);
}

int isEOF(char * line){
    if (strcmp(line,"EOF")==0)
    {
        return 1;
    }
    return 0;
}

void push(int consocket, char * project, char * buffer){
    char * commitfile = getToken(buffer, 0);
    int i, j, k = 0;
    int l = 0;
    int commitIndex = getCommitVersion(project);
    commitIndex++;
    for (i = 0; i < commitIndex; i++){
        char * commitv = newString(8+numOfD(i));
        sprintf(commitv,".Commit%d",i);
        char * commitpath = getAbsolutePath(project, commitv);
        int fd = open(commitpath,O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
        check(fd);
        char * serv_commit = readfile(fd);
        if (strcmp(commitfile, serv_commit)==0){
            free(commitv);
            free(commitpath);
            close(fd);
            break;
        }
        free(commitv);
        free(commitpath);
        close(fd);
    }
    if (i == commitIndex){
        //error no same commit file
        puts("Push Failed: No matching commit file");
        check(send(consocket, "Failed", 7,0));
        return;
    }
    check(send(consocket, "Success!", 9,0));
    char * historypath = getAbsolutePath(project,".History");
    int fd2 = open(historypath,O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
    check(fd2);
    char * vers = getCurrentVersion(project);
    write(fd2,vers,strlen(vers));
    write(fd2,"\n",1);
    write(fd2,commitfile,strlen(commitfile));
    write(fd2,"\n",1);
    free(vers);
    close(fd2);
    for (j = 0; j < commitIndex; j++){
        if (i!=j){
            char * commitv = newString(8+numOfD(i));
            sprintf(commitv,".Commit%d",commitIndex);
            char * commitpath = getAbsolutePath(project, commitv);
            remove(commitpath);
            free(commitpath);
            free(commitv);
        }
    }
    char * currentv = getCurrentVersion(project);
    char * currentdir = getAbsolutePath(project, currentv);
    int nv = atoi(currentv)+1;
    char * newv = newString(numOfD(nv)+1);
    sprintf(newv, "%d", nv);
    char * newdir = getAbsolutePath(project, newv);
    copyDirectory(currentdir,newdir);
    char * commitline = getLine(commitfile, k);
    while(!isEOF(commitline)){
        char * command = getFilename(commitline);
        char * commFilename = getVersion(commitline);
        char * commpath = getAbsoluteServerPath(project,commFilename);
        if (strcmp("A",command)==0 || strcmp("M", command)==0){
            int commfd = open(commpath,O_RDWR|O_CREAT|O_APPEND|O_TRUNC, S_IRUSR | S_IWUSR);
            if (commfd == -1){
                createDirectories(commpath);
                commfd = open(commpath,O_RDWR|O_CREAT|O_APPEND|O_TRUNC, S_IRUSR | S_IWUSR);
            }
            check(commfd);
            char * tempBuff = strstr(buffer+strlen(commitfile)+1,commFilename);
            char * fsize = getToken(tempBuff+strlen(commFilename)+1,0);
            int size = atoi(fsize);
            char * newFile = newString(size+1);
            strncpy(newFile, tempBuff+strlen(commFilename)+strlen(fsize)+2, size);
            write(commfd, newFile, size);
            close(commfd);
            free(fsize);
            free(newFile);
        }
        else if(strcmp("D",command)==0){
            remove(commpath);
        }
        free(command);
        free(commFilename);
        free(commpath);
        k++;
        commitline = resetLine(commitline, commitfile, k);    
    }
    char * cli_manifest = readsocket(consocket);
    char * servline = getLine(cli_manifest, l);
    char * manpath = getAbsoluteServerPath(project, ".Manifest");
    char * temppath = getAbsoluteServerPath(project, ".ManifestTemp");
    int fdtemp = open(temppath,O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR); 
    int fdman = open(manpath,O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
    check(fdtemp);
    check(fdman);
    char * serv_manifest = readfile(fdman);
    
    while(!isEOF(servline)){
        if (l == 0){
            write(fdtemp, servline, strlen(servline));
	    write(fdtemp,"\n",1);
        }
        else if (l==1){
            char * serv_v = getLine(serv_manifest, l);
            int nvers = atoi(serv_v)+1;
            char * serv_nv = newString(numOfD(nvers)+1);
            sprintf(serv_nv, "%d", nvers);
            write(fdtemp, serv_nv, strlen(serv_nv));
	    write(fdtemp,"\n",1);
        }
        else{
            char * toBeD = newString(7);
            char * filename = getFilename(servline);
            strncat(toBeD, filename, 6);
            if (strcmp(toBeD,"addlc>")==0){
                write(fdtemp, servline+6, strlen(servline)-6);
		write(fdtemp, "\n", 1);
            }
            else if (strcmp(toBeD,"remlc>")!=0){
                write(fdtemp, servline, strlen(servline));
	        write(fdtemp, "\n", 1);
            }
        }
        l++;
        servline = resetLine(servline, cli_manifest, l);
    }
    close(fdtemp);
    close(fdman);
    remove(manpath);
    rename(temppath, manpath);
    fdman = open(manpath,O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
    check(fdman);
    free(serv_manifest);
    serv_manifest = readfile(fdman);
    check(send(consocket, serv_manifest, strlen(serv_manifest)+1,0));
    close(fdman);
}

void update(int consocket, char * project){
    char * manifest = readManifest(project);
    check(send(consocket,manifest,strlen(manifest)+1,0));
}
void upgrade(int consocket, char * project, char * buf){
  int projectlen = strlen(project);
  sendfiles(consocket, buf+(projectlen+1),project);
}

void currentversion(int consocket,char * project){
    int len = MAXRCVLEN+1;
    int linelen, start = 0;
    int line = 0;
    char * cversion = newString(len);
    char * manifest = readManifest(project);

    do{
        linelen = strcspn(manifest+start, "\n");
        if(linelen>0){
            if (line > 1){
                int filelen = strcspn(manifest+start, " ");
                int versionlen = strcspn(manifest+start+filelen+1," ");
                int tokenlen = filelen+versionlen+1;
                char * token = newString(tokenlen+2);
                strncpy(token, manifest+start,tokenlen);
                token[tokenlen]='\n';
                if(strlen(cversion)+strlen(token) >= len){
                    cversion = resize(cversion,len);
                    len = relen(len);
                }
                strcat(cversion,token);
                free(token);
            }
            else{
                char * token = newString(linelen+1);
                strncpy(token, manifest+start, linelen);
                if(strlen(cversion)+strlen(token)+2 > len){
                    cversion = resize(cversion,len);
                    len = relen(len);
                }
                strcat(cversion,token);
                strcat(cversion,"\n");
                free(token);
            }
            line++;
        }
        start+=linelen+1;
    }while(start<strlen(manifest));
    check(send(consocket,cversion,strlen(cversion)+1,0));
    //printf("%s\n",cversion);
    free(manifest);
    free(cversion);
}

void * server_handler(void * new_sock){
 
    int consocket = *((int*)new_sock);
    free(new_sock);
    char * buffer = readsocket(consocket);
    //newString(MAXRCVLEN+1);
    //int len = recv(consocket, buffer, MAXRCVLEN, 0);
    //printf("Received %s (%d bytes).\n", buffer, strlen(buffer));

    char * command = getToken(buffer,0);
    char * projectname = getToken(buffer,strlen(command)+1);
    lock(projectname);
    if(strcmp(command,"create")==0){
        char * manifest = createProject(projectname);
        if(strcmp(manifest,"")==0){
            check(send(consocket,"failure",8,0));
        }
        else{
            check(send(consocket,manifest,strlen(manifest)+1,0));
        }
        free(manifest);
    }
    else if(strcmp(command,"checkout")==0){
	   checkout(consocket,projectname);
	}
    else if(strcmp(command,"currentversion")==0){
	   char * projectname = getToken(buffer,15);
	   currentversion(consocket,projectname);
	}
	else if(strcmp(command,"update")==0){
	  char * projectname = getToken(buffer,7);
	   update(consocket,projectname);
	}
    else if(strcmp(command,"commit")==0){
	  char * projectname = getToken(buffer,7);
	   commit(consocket,projectname);
	}
    else if(strcmp(command,"upgrade")==0){
	  char * projectname = getToken(buffer,8);
	  upgrade(consocket,projectname, buffer + 8);
	}
    else if(strcmp(command,"push")==0){
        char * projectname = getToken(buffer,5);
        push(consocket,projectname, buffer + 5 + strlen(projectname)+1);
	}
    else if(strcmp(command,"destroy")==0){
        char * projectname = getToken(buffer,8);
        destroy(consocket, projectname);
        free(projectname);
	}
    else if(strcmp(command,"history")==0){
        char * projectname = getToken(buffer,8);
        history(consocket,projectname);
	}
    else if(strcmp(command,"rollback")==0){
        char * projectname = getToken(buffer,9);
        rollback(consocket,projectname,buffer + 9 + strlen(projectname)+1);
	}
    unlock(projectname);
    free(projectname);
    free(command);
    memset(buffer, '\0', strlen(buffer));
    close(consocket);
}

int main(int argc, char *argv[])
{
  
    struct sockaddr_in dest;                                                            // socket info about the machine connecting to us
    
    struct sockaddr_in serv;                                                            // socket info about our server 
    int len, portnum,consocket, mysocket;                                               // socket used to listen for incoming connections

    if (argc != 2){
        printf("Error: Insufficient number of arguments\n");
        exit(0);
    }

    if (atoi(argv[1]) <= 0 && strcmp(argv[1], "0") != 0){
        printf("Error: Invalid port number\n");
        exit(0);
    }

    portnum = atoi(argv[1]);
    socklen_t socksize = sizeof(struct sockaddr_in);
    memset(&serv, 0, sizeof(serv));                                                     // zero the struct before filling the fields 
    serv.sin_family = AF_INET;                                                          // set the type of connection to TCP/IP 
    serv.sin_addr.s_addr = htonl(INADDR_ANY);                                           // set our address to any interface 
    serv.sin_port = htons(PORTNUM);                                                     // set the server port number 
    mysocket = socket(AF_INET, SOCK_STREAM, 0);
    check(mysocket);
    check(bind(mysocket, (struct sockaddr *)&serv, sizeof(struct sockaddr)));                  // bind serv information to mysocket 
    check(listen(mysocket, 1));                                                                // start listening, allowing a queue of up to 1 pending connection 
    puts("Server is online!\nWaiting for Connection...");
    
    while(consocket= accept(mysocket, (struct sockaddr *)&dest, &socksize)){
        check(consocket);
        printf("Incoming connection from %s - sending welcome\n", inet_ntoa(dest.sin_addr));
        pthread_t server_thread;
        int * new_sock = malloc(sizeof(int));
        *new_sock = consocket;
        pthread_create(&server_thread, NULL, server_handler, (void*)new_sock);
    }
    
    close(mysocket);
    return 0;
}
