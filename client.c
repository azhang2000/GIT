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
#include <stdarg.h>

#define MAXRCVLEN 500
#define PORTNUM 2343

void check(int n){
    if (n==-1){
        perror("Error");
        exit(1);
    }
}

void freeN(int n, ...){
    va_list args;
    va_start(args, n);
    int i;
    for (i = 0; i < n; i++){
        char * vp = va_arg(args,char *);
        free(vp);
    }
    va_end(args);
}

void closeN(int n, ...){
    va_list args;
    va_start(args, n);
    int i;
    for (i = 0; i < n; i++){
        int vp = va_arg(args,int);
        close(vp);
    }
    va_end(args);
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

char * newString(int len){
    char * buffer = (char *)malloc(len * sizeof(char));
    memset(buffer, '\0', len);
    return buffer;
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

char * getHash(char * string, int length){
    int n;
    MD5_CTX c;
    unsigned char digest[16];
    char * out = newString(33);

    MD5_Init(&c);

    while (length > 0) {
        if (length > 512) {
            MD5_Update(&c, string, 512);
        } 
        else {
            MD5_Update(&c, string, length);
        }
        length -= 512;
        string += 512;
    }
    
    MD5_Final(digest, &c);
    for (n = 0; n < 16; ++n) {
        snprintf(&(out[n*2]), 16*2, "%02x", (unsigned int)digest[n]);
    }
    return out;
}

char * getToken(char * buffer, int start, char * delim){
    int i = strcspn(buffer+start, delim);
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

void receivefiles(char * protocol, char * project){
  char * numfiles = getToken(protocol,5,":");
    int fileno = 0;
    int i = 6 + strlen(numfiles);
    
    while(fileno<atoi(numfiles)){
      char * file = getToken(protocol,i,":");
        i = i + strlen(file)+1;
        
        char * fsize = getToken(protocol,i,":");
        i = i + strlen(fsize)+1;
        
        int filelen = atoi(fsize);
        char * fcontents = newString(filelen+1);
        strncpy(fcontents,protocol+i,filelen);
        
        i=i+filelen+1;
        
        int fd = open(file,O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
        if(fd==-1){
	        createDirectories(file);
            fd = open(file,O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
	    }
        check(fd);
	    write(fd,fcontents,strlen(fcontents));
        close(fd);
        free(fcontents);
        free(fsize);
        free(file);
        fileno++;
    }

    free(numfiles);
}

char * requestfiles(int mysocket, char * files){
    check(send(mysocket,"request:",8,0));
    check(send(mysocket,files,strlen(files)+1,0));
    char * protocol = readsocket(mysocket);
    receivefiles(protocol,"");
    free(protocol);
    return "success";
}

void sendfiles(int mysocket, char * files){
    int linelen, start = 0;
    int len = MAXRCVLEN+1;
    int filenum=0;
    char * result = newString(len);
    
    do{
        linelen = strcspn(files+start, "\n");
        if(linelen>0){
            char * file = newString(linelen+1);
            strncpy(file, files+start,linelen);
            int fd = open(file,O_RDONLY);
            check(fd);
            filenum++;
            char * filecontents = readfile(fd);
            char * size = newString(numOfD(strlen(filecontents))+1);
            sprintf(size,"%d",strlen(filecontents));
            if ((strlen(result)+strlen(filecontents) + strlen(file) + strlen(size) + 3) >= len){ // +3 for the : delimiter
                result = resize(result, len);
                len = relen(len);
            }
            strcat(result,file);
            strcat(result,":");
            strcat(result,size);
            strcat(result,":");
            strcat(result,filecontents);
            strcat(result,":");
            free(filecontents);
            free(file);
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
    check(send(mysocket,protocol,strlen(protocol)+1,0));
    free(result);
    free(numfile);
    free(protocol);
}

void createProject(int mysocket, char * project){
    check(send(mysocket,"create:",7,0));
    check(send(mysocket,project,strlen(project)+1,0));
    char * status = readsocket(mysocket);
    if(strcmp(status,"failure")==0){
        printf("Error: Project already exists.\n");
    }
    else{
        printf("Success\n");
        mkdir(project, 00700);
        char * path = getAbsolutePath(project,".Manifest");
        int fd = open(path,O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
        check(fd);
        free(path);
        write(fd,status,strlen(status));
        close(fd);
    }
    free(status);
}

void checkout(int mysocket, char * project){
    struct stat st = {0};

    if (stat(project, &st) != -1) {
        printf("Error: project already exists\n");
        return;
    }
    else{
        mkdir(project, 00700);
        check(send(mysocket,"checkout:",9,0));
        check(send(mysocket,project,strlen(project),0));
        check(send(mysocket,":",2,0));
        char * protocol = readsocket(mysocket);
        if (strcmp(protocol, "failure")==0){
            puts("Checkout Failed.");
            exit(1);
        }
        receivefiles(protocol,project);
        puts("Success!");
        free(protocol);
    }
}

void currentversion(int mysocket, char * project){
    check(send(mysocket,"currentversion:",15,0));
    check(send(mysocket,project,strlen(project),0));
    check(send(mysocket,":",2,0));
    char * current = readsocket(mysocket);
    printf("%s\n",current);
    free(current);
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

void writeLine(int fd, char * command, char * hashpath, char * hash){
    write(fd,command,strlen(command));
    write(fd,hashpath,strlen(hashpath));
    write(fd," ",1);
    write(fd,hash,strlen(hash));
    write(fd,"\n",1);
    printf("%s%s\n",command, hashpath);
}

void sendLine(int fd, char * command, char * hashpath, char * hash){
    write(fd,command,strlen(command));
    write(fd,hashpath,strlen(hashpath));
    write(fd," ",1);
    write(fd,hash,strlen(hash));
    write(fd,"\n",1);
}

int isEOF(char * line){
    if (strcmp(line,"EOF")==0)
    {
        return 1;
    }
    return 0;
}
void history(int mysocket, char * project){
  check(send(mysocket,"history:",8,0));
  check(send(mysocket,project,strlen(project),0));
  check(send(mysocket,":",2,0));
  char * status = readsocket(mysocket);
  if (strcmp(status,"Success!")!=0){
      free(status);
      puts("Failed: Project does not exist");
      return;
  }
  free(status);
  char * history = readsocket(mysocket);
  printf("History: \n%s\n", history);
  free(history);
}

void destroy(int mysocket, char * project){
  check(send(mysocket,"destroy:",8,0));
  check(send(mysocket,project,strlen(project),0));
  check(send(mysocket,":",2,0));
  char * status = readsocket(mysocket);
  if (strcmp(status,"Success!")!=0){
      free(status);
      puts("Failed: Project does not exist");
      return;
  }
  free(status);
      puts("Success!");
}

void rollback(int mysocket, char * project, char * version){
  check(send(mysocket,"rollback:",9,0));
  check(send(mysocket,project,strlen(project),0));
  check(send(mysocket,":",1,0));
  check(send(mysocket,version,strlen(version),0));
  check(send(mysocket,":",2,0));
    char * status = readsocket(mysocket);
    if (strcmp(status,"Success!")!=0){
      free(status);
      puts("Rollback Failed: Incorrect Project Name or Version");
      return;
    }
    free(status);
    puts("Success!");
}

void commitProject(int mysocket, char * project){
    check(send(mysocket,"commit:",7,0));
    check(send(mysocket,project, strlen(project),0));
    check(send(mysocket,":",2,0));
    char * serv_manifest = readsocket(mysocket);
    if (strcmp(serv_manifest,"failure")==0){
        puts("Commit Failed");
    }
    else {
        puts("Commiting");
        int i = 1;
        char * path = getAbsolutePath(project,".Manifest");
        char * updmanpath = getAbsolutePath(project, ".tempManifest");
        char * commitpath = getAbsolutePath(project, ".Commit");
        char * updatepath = getAbsolutePath(project, ".Update");
        char * conflictpath = getAbsolutePath(project, ".Conflict");
        int fdpath = open(path,O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
        int fdcommit = open(commitpath,O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
        int fdupdate = open(updatepath,O_RDWR|O_APPEND, S_IRUSR | S_IWUSR);
        int fdconflict = open(conflictpath,O_RDONLY, S_IRUSR | S_IWUSR);
        int fdman = open(updmanpath, O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
        check(fdpath);
        check(fdcommit);
        check(fdman);
        char * cli_manifest = readfile(fdpath);
        i = 0;
        char * cli_line = getLine(cli_manifest, i);
        write(fdman, cli_line, strlen(cli_line));
	write(fdman,"\n",1);
        i++;
	free(cli_line);
        cli_line = getLine(cli_manifest, i);
        char * serv_line = getLine(serv_manifest,i);

	
        if (isEOF(cli_line)){
            remove(updmanpath);
            puts("Error: Incomplete Client Manifest");
            freeN(9, updmanpath, path, updatepath,conflictpath,commitpath, cli_line, serv_line, cli_manifest, serv_manifest);
            closeN(6, fdman, fdpath, fdupdate, fdconflict, fdcommit, mysocket);
            exit(0);
        }
        if (atoi(cli_line)!=atoi(serv_line)){
            remove(updmanpath);    
            puts("Failed: Client and Server Version are not the same");
            freeN(9, updmanpath, path, updatepath,conflictpath,commitpath, cli_line, serv_line, cli_manifest, serv_manifest);
            closeN(6, fdman, fdpath, fdupdate, fdconflict, fdcommit, mysocket);
            return;
        }
        if(access(conflictpath,F_OK)!=-1){
            remove(updmanpath);
            printf("Failed: .Conflict Exists\n");
            freeN(9, updmanpath, path, updatepath,conflictpath,commitpath, cli_line, serv_line, cli_manifest, serv_manifest);
            closeN(6, fdman, fdpath, fdupdate, fdconflict, fdcommit, mysocket);
            return;
        }
        if (access(updatepath,F_OK)!=-1){
            char * updateFile = readfile(fdupdate);
            if (strlen(updateFile)!=0){
                remove(updmanpath);
                printf("Failed: .Update is not empty\n");
                freeN(10, updmanpath, path, updatepath,conflictpath,commitpath, cli_line, serv_line, cli_manifest, serv_manifest, updateFile);
                closeN(6, fdman, fdpath, fdupdate, fdconflict, fdcommit, mysocket);
                return;
            }
        }
        write(fdman, cli_line, strlen(cli_line));
	    write(fdman,"\n",1);
        i = 2;
        cli_line = resetLine(cli_line, cli_manifest, i);
        while (!isEOF(cli_line)){
            char * filename = getFilename(cli_line);
            char * hash = getFileHash(cli_line);
            char * toBeD = newString(7);
            int change = 0;
            strncat(toBeD, filename, 6);
            if (strcmp(toBeD,"addlc>")==0){
                char * hashpath = filename+6;
                sendLine(mysocket, "A ", hashpath, hash);
                writeLine(fdcommit, "A ", hashpath, hash);
            }
            else if(strcmp(toBeD,"remlc>")==0){
                char * hashpath = filename+6;
                sendLine(mysocket, "D ", hashpath, hash);                
                writeLine(fdcommit, "D ", hashpath, hash);
            }   
            else{
                if (strstr(serv_manifest,filename)!=NULL){
                    serv_line = resetLine(serv_line, strstr(serv_manifest,filename),0);
                    char * serv_hash = getFileHash(serv_line);
                    if (strcmp(hash,serv_hash)==0){
                        char * hashpath = getAbsolutePath(project, filename);
                        int hashfd = open(hashpath,O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
                        check(hashfd);
                        char * hashfile = readfile(hashfd);
                        char * livehash = getHash(hashfile,strlen(hashfile));
                        if (strcmp(hash,livehash)!=0){
                            sendLine(mysocket, "M ", filename, hash);
                            writeLine(fdcommit, "M ", filename, hash);
                            change = 1;
                            write(fdman, filename, strlen(filename));
                            write(fdman, " ", 1);
                            char * cver = getVersion(cli_line);
                            int nv = atoi(cver)+1;
                            char * nver = newString(numOfD(nv)+2);
                            sprintf(nver, "%d ", nv);
                            write(fdman, nver, strlen(nver));
                            write(fdman, livehash, strlen(livehash));
                            freeN(2, cver, nver);
                        }
                        close(hashfd);
                        freeN(3, hashpath, hashfile, livehash);
                    }
                    else{
                        puts("Failed: Server and Client Hash Are Different");
                        freeN(13, updmanpath, path, updatepath,conflictpath,commitpath, cli_line, serv_line, cli_manifest, serv_manifest, filename, toBeD, hash, serv_hash);
                        closeN(6, fdman, fdpath, fdupdate, fdconflict, fdcommit, mysocket);
                        return;     
                    }
                    free(serv_hash);
                }
                else{
                    puts("Failed: Server doesn't have a file client does, but the file isn't new");
                    freeN(12, updmanpath, path, updatepath,conflictpath,commitpath, cli_line, serv_line, cli_manifest, serv_manifest, filename, toBeD, hash);
                    closeN(6, fdman, fdpath, fdupdate, fdconflict, fdcommit, mysocket);
                    return; 
                }
            } 
            if (change==0){
                write(fdman, cli_line,strlen(cli_line));
            }
            write(fdman, "\n",1);
                
            i++;
            freeN(3, hash, filename, toBeD);
            cli_line = resetLine(cli_line, cli_manifest, i);
        }
	    //char * commitFile = readfile(fdcommit);
        //send(mysocket,commitFile, strlen(commitFile)+1, 0);
        write(mysocket, "\0", 1);
        remove(path);
        rename(updmanpath, path);
        puts("Success!");
        freeN(9, updmanpath, path, updatepath,conflictpath,commitpath, cli_line, serv_line, cli_manifest, serv_manifest);
        closeN(5, fdman, fdpath, fdupdate, fdconflict, fdcommit);
    }
}

void updateManifest(char * version, char * project){
    char * manifestpath = getAbsolutePath(project,".Manifest");
    int fd = open(manifestpath, O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
    check(fd);
    char * manifest = readfile(fd);
    int linelen, start = 0;
    char * temppath = getAbsolutePath(project,"temp");
    int fd2 = open(temppath,O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
    check(fd2);
    write(fd2,project,strlen(project));
    write(fd2,"\n",1);
    write(fd2,version,strlen(version));
    write(fd2,"\n",1);
    int i = 2;
    char * serv_line = getLine(manifest,i);
    while(!isEOF(serv_line)){
      write(fd2, serv_line,strlen(serv_line));
      write(fd2, "\n",1);
      i++;
      free(serv_line);
      serv_line = getLine(manifest,i);
    }
    free(serv_line);
    remove(manifestpath);
    rename(temppath,manifestpath);
    close(fd);
    close(fd2);
    free(manifestpath);
    free(manifest);
}

void updateProject(int mysocket, char * project){
    check(send(mysocket,"update:",7,0));
    check(send(mysocket,project, strlen(project),0));
    check(send(mysocket,":",2,0));
    char * serv_manifest = readsocket(mysocket);
    if (strcmp(serv_manifest,"failure")==0){
        puts("Update Failed.");
    }
    else {
        puts("Updating");
        int i = 1;
        int flag = 0;
        char * path = getAbsolutePath(project,".Manifest");
        char * updatepath = getAbsolutePath(project, ".Update");
        char * conflictpath = getAbsolutePath(project, ".Conflict");
	char * tempManifest = getAbsolutePath(project,".tManifest");
	int fdtManifest = open(tempManifest,O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
    check(fdtManifest);

	write(fdtManifest,serv_manifest,strlen(serv_manifest));
        int fdpath = open(path,O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
        int fdupdate = open(updatepath,O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
        int fdconflict = open(conflictpath,O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
        check(fdpath);
        check(fdupdate);
        check(fdconflict);
        
        char * cli_manifest = readfile(fdpath);
        i = 1;
        char * cli_line = getLine(cli_manifest, 1);
	    char * serv_line = getLine(serv_manifest,1);
	    if (strcmp(cli_line,serv_line)==0){
            if (isEOF(cli_line)){
                puts("error");
                exit(0);
            }
            remove(conflictpath);
            int clear = open(updatepath,O_RDWR|O_TRUNC);
            puts("Up to Date");
            freeN(3, path, updatepath, conflictpath);
            closeN(4, fdpath, fdupdate, fdconflict, clear);
            return;
        }
        write(fdupdate,serv_line, strlen(serv_line));
        write(fdupdate, "\n", 1);
        i++;
        serv_line = getLine(serv_manifest,i);
	    while(!isEOF(serv_line)){
            if (strlen(serv_line) > 0){
                char * filename = getFilename(serv_line);
                char * hashpath = getAbsolutePath(project,filename);
                char * serv_hash = getFileHash(serv_line);
                if (strstr(cli_manifest,filename)!=NULL){                                      // Server File Found In Client Manifest
		  cli_line = getLine( strstr(cli_manifest,filename), 0);
                    char * cli_hash = getFileHash(cli_line);
		    char * filename2 = getFilename(cli_line);
		    char * toBeD = newString(7);
		    strncat(toBeD, filename2, 6);

                    if (strcmp(serv_hash, cli_hash)!=0  && strcmp(toBeD,"remlc>")!=0){                                        // Server Hash Diff Than Client Hash
                        int hashfd = open(hashpath,O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
                        check(hashfd);
                        char * hashfile = readfile(hashfd);
                        char * livehash = getHash(hashfile,strlen(hashfile));
                        if (strcmp(livehash, cli_hash)!=0){                                     // Client Hash Diff Than Live Hash - C
                            flag = 1;
                            writeLine(fdconflict,"C ", filename, livehash);
                            remove(updatepath);
			    remove(tempManifest);
                        }
                        else{                                                                   // Client Hash Same As Live Hash - M
                            if (flag==0){
                                writeLine(fdupdate, "M ", filename, livehash);
                            }
                        }
                        freeN(2, livehash, hashfile);
                        close(hashfd);
                    }
		    free(toBeD);
		    free(filename2);
                    freeN(2, serv_hash, cli_hash);
                }
                else{                                                                           // Server File Not In Client Manifest
                    if (flag==0){ 
                        writeLine(fdupdate, "A ", filename, serv_hash);
                    }
                }
		
                freeN(1, hashpath);
            }
            i++;
            serv_line = getLine(serv_manifest,i);
        }
        if (flag == 0) {
	  remove(conflictpath);
	    i = 2;
            cli_line = getLine(cli_manifest, i);
            while (!isEOF(cli_line)){
                char * filename = getFilename(cli_line);
                if (strstr(serv_manifest,filename)==NULL){
                    char * toBeD = newString(7);
                    strncat(toBeD, filename, 6);
                    if (strcmp(toBeD,"addlc>")!=0 && strcmp(toBeD,"remlc>")!=0){
                        char * hashpath = getAbsolutePath(project,filename);
                        char * hash = getFileHash(cli_line);
                        writeLine(fdupdate, "D ", filename, hash);
                        freeN(2, hashpath, hash);
                    }
		    else{
		      write(fdtManifest,cli_line,strlen(cli_line));
		      write(fdtManifest,"\n",1);
		      }
                    free(toBeD);
                }
                i++;
                free(filename);
                cli_line = getLine(cli_manifest, i);
            }
        }
        freeN(6, cli_line, serv_line, cli_manifest, path, updatepath, conflictpath);
        closeN(4, fdpath, fdupdate, fdconflict, fdtManifest);
    }
    free(serv_manifest);
    printf("success!\n");
    return;
}

void pushfiles(int mysocket, char * files, char * project){
    int linelen, start = 0;
    int len = MAXRCVLEN+1;
    int filenum=0;
    char * result = newString(len);
    
    do{
        linelen = strcspn(files+start, "\n");
        if(linelen>0){
            char * file = newString(linelen+1);
            strncpy(file, files+start,linelen);
            char * absfile = getAbsolutePath(project,file);
            int fd = open(absfile,O_RDONLY);
            check(fd);
            filenum++;
            char * filecontents = readfile(fd);
            char * size = newString(numOfD(strlen(filecontents))+1);
            sprintf(size,"%d",strlen(filecontents));
            if ((strlen(result)+strlen(filecontents) + strlen(file) + strlen(size) + 3) >= len){ // +3 for the : delimiter
                result = resize(result, len);
                len = relen(len);
            }
            strcat(result,file);
            strcat(result,":");
            strcat(result,size);
            strcat(result,":");
            strcat(result,filecontents);
            strcat(result,":");
            free(absfile);
            free(filecontents);
            free(file);
            free(size);
            close(fd);
        }
        start+=linelen+1;
    } while(start<strlen(files)); 
    char * numfile = newString(numOfD(filenum)+1);
    sprintf(numfile,"%d",filenum);
    char * protocol = newString(2+strlen(result)+strlen(numfile));
    strcat(protocol,numfile);
    strcat(protocol,":");
    strcat(protocol,result);
    printf("%s\n",protocol);
    send(mysocket,protocol,strlen(protocol)+1,0);
    free(result);
    free(numfile);
    free(protocol);
}

char * parseCommit(char * commit){
    int linelen = 0;
    int start = 0;
    int len = MAXRCVLEN + 1;
    int filenum = 0;;
    char * result = newString(len);
    do {
        linelen = strcspn(commit+start, "\n");
        if (linelen>0){
            char * cline = newString(linelen+1);
            strncpy(cline, commit+start, linelen);
            char * filename = getVersion(cline);
            if (strlen(result)+strlen(filename)+1>=len){
                result=resize(result,len);
                len = relen(len);
            }
            strcat(result, filename);
            strcat(result,"\n");
            freeN(2,cline,filename);
        }
        start+=linelen+1;
    } while(start<strlen(commit));
    return result;
}

void pushProject(int mysocket, char * project){
    check(send(mysocket,"push:",5,0));
    check(send(mysocket,project, strlen(project),0));
    check(send(mysocket, ":", 1, 0));
    char * commitpath = getAbsolutePath(project, ".Commit");
    int fdcommit = open(commitpath,O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
    check(fdcommit);
    char * commitfile = readfile(fdcommit);
    check(send(mysocket,commitfile,strlen(commitfile),0));
    check(send(mysocket,":",1,0));
    char * files = parseCommit(commitfile);
    pushfiles(mysocket,files,project);
    char * response = readsocket(mysocket);
    if (strcmp(response,"Success!")!=0){
        puts("Push Failed");
        return;
    }
    char * manifestpath = getAbsolutePath(project, ".Manifest");
    int fdman = open(manifestpath,O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
    check(fdman);
    char * cli_manifest = readfile(fdman);
    close(fdman);
    check (send(mysocket, cli_manifest, strlen(cli_manifest)+1,0));
    char * serv_manifest = readsocket(mysocket);
    remove(commitpath);
    remove(manifestpath);
    int fdman2 = open(manifestpath,O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
    check(fdman2);
    write(fdman2,serv_manifest,strlen(serv_manifest));
    close(fdman2);
    close(fdcommit);
    printf("Success!\n");
}

void upgrade(int mysocket, char * project){
 
  char * updatepath = getAbsolutePath(project,".Update");
  char * conflictpath = getAbsolutePath(project,".Conflict");
  if(access(conflictpath,F_OK)!=-1){
    printf("Error: Fix conflicts first\n");
    return;
  }
  if(access(updatepath,F_OK)==-1){
    printf("Error: No updates required\n");
    return;
  }
  check(send(mysocket,"upgrade:",8,0));
  check(send(mysocket,project, strlen(project),0));
  check(send(mysocket,":",1,0));
  int fd = open(updatepath,O_RDONLY);
  check(fd);
  char * update = readfile(fd);

  if(strlen(update)==0){
    printf("Empty upgrade file, no updates needed!\n");
    return;
  }
  char * versionNum = getLine(update,0);
  //printf("%s\n",update);
  int i = 1;
  char * serv_line = getLine(update,i);
  while(!isEOF(serv_line)){
    if(serv_line[0]=='D'){
      //delete file on client side
      char * file = getToken(serv_line,2," ");
      char * absolutepath = getAbsolutePath(project,file);
      printf("D %s\n",absolutepath);
      remove(absolutepath);
      free(file);
      free(absolutepath);
    }
    else if(serv_line[0]=='A'){
      //request file to be downloaded on server side
      char * file = getToken(serv_line,2," ");
      char * absolutepath = getAbsolutePath(project,file);
      printf("A %s\n",absolutepath);
      check(send(mysocket,file,strlen(file),0));
      check(send(mysocket,"\n",1,0));
      free(file);
      free(absolutepath);
    }
    else if(serv_line[0]=='M'){
      char * file = getToken(serv_line,2," ");
      char * absolutepath = getAbsolutePath(project,file);
      printf("M %s\n",absolutepath);
      remove(absolutepath);
      check(send(mysocket,file,strlen(file),0));
      check(send(mysocket,"\n",1,0));
      free(file);
      free(absolutepath);
      //delete file on client side
      //request file to be downloaded on server side
    }
    i++;
    free(serv_line);
    serv_line = getLine(update,i);
  }
  check(send(mysocket,"",1,0));
  char * received = readsocket(mysocket);
  printf("Upgrading...\n");
  receivefiles(received,project);
  //update manifest
  //updateManifest(versionNum,project);
  char * tempManifest = getAbsolutePath(project,".tManifest");
  char * manifestpath = getAbsolutePath(project,".Manifest");
  remove(manifestpath);
  rename(tempManifest,manifestpath);
  remove(updatepath);
  puts("Success!");
  close(fd);
  //read socket
  //call receivefiles 
  free(updatepath);
  free(conflictpath);
}
void addFile(char * project, char * file){
    struct dirent * dp;
    DIR * dir = opendir(project);
    char * filepath = getAbsolutePath(project,file); 
    printf("%s\n",filepath);
    if(dir!=NULL){	     
        if(access(filepath,F_OK)!=-1){
            int fd = open(filepath,O_RDONLY);
            check(fd);
            char * filecontents = readfile(fd);
            char * hash = getHash(filecontents,strlen(filecontents));
            char * manifestpath = getAbsolutePath(project, ".Manifest");
            int fd2 = open(manifestpath, O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
            check(fd2);
            char * manifest = readfile(fd2);
            char * localfile = newString(strlen(file)+8);
            strcat(localfile,"addlc>");
            strcat(localfile,file);
            strcat(localfile, " ");
            if(strstr(manifest,localfile)!=NULL){
                printf("Error, file already added\n");
                return;
            }
            write(fd2,"addlc>",6);
            write(fd2, file, strlen(file));
            write(fd2, " 0 ",3);
            write(fd2,hash,strlen(hash));
            write(fd2,"\n",1);
            printf("Added!\n");
            free(hash);
            free(filecontents);
            free(manifestpath);
            close(fd);
            close(fd2);
            return;
        }

        printf("Error: File not found\n");
	    free(filepath);
    }
    else
    {
        printf("Error: project not found\n");
    }
}


void removeFile(char * project, char * rfile){
    char * manifestpath = getAbsolutePath(project,".Manifest");
    int fd = open(manifestpath, O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
    check(fd);
    char * manifest = readfile(fd);
    int linelen, start = 0;
    char * temppath = getAbsolutePath(project,"temp");
    int fd2 = open(temppath,O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
    check(fd2);
    char * localfile = newString(strlen(rfile)+7);
    strcat(localfile,"addlc>");
    strcat(localfile,rfile);
    do{
        linelen = strcspn(manifest+start, "\n");
        if(linelen>0){
            char * lineM = newString(linelen+1);
            strncpy(lineM, manifest+start, linelen);
            char * file = getFilename(lineM);
            if(strcmp(file,rfile)==0 || strcmp(file,localfile)==0){
                char * toBeD = newString(7);
                strncat(toBeD, file, 6);
		if(strcmp(toBeD,"addlc>")!=0){
	        write(fd2,"remlc>",6);
		write(fd2,lineM,strlen(lineM));
                write(fd2,"\n",1);
		}
		free(toBeD);
		printf("Removed!\n");
            }
	    else{
            write(fd2,lineM,strlen(lineM));
            write(fd2,"\n",1);
	    }
            free(lineM);            
            free(file);
        }
        start+=linelen+1;
    } while(start<strlen(manifest));
    remove(manifestpath);
    rename(temppath,manifestpath);
    close(fd);
    close(fd2);
    free(manifestpath);
    free(manifest);
    free(localfile);
}

void configure(char * IP, char * port){
    int fd = open(".configure", O_RDWR|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
    check(fd);
    write(fd,IP,strlen(IP));
    write(fd,":",1);
    write(fd,port,strlen(port));
    write(fd,":",1);
    close(fd);
}

int getPort(){
    int fd = open(".configure",O_RDONLY);
    if(fd==-1){
      printf("error: no config file\n");
      exit(1);
    }
    char * configure = readfile(fd);
    char * IP = getToken(configure,0,":");
    char * port = getToken(configure,strlen(IP)+1,":");
    int portno = atoi(port);
    free(IP);
    free(port);
    close(fd);
    return portno;
}

void checkArgs(int arg, int n){
    if (arg < n + 1){
        puts("Error: Insufficient Arguments");
        exit(1);
    }
}

int main(int argc, char *argv[])
{
    int portnum, mysocket;
    struct sockaddr_in dest;

    checkArgs(argc, 2);
    if(strcmp(argv[1],"configure")==0){
        checkArgs(argc, 3);
        configure(argv[2],argv[3]);
        puts("Success!");
        return 0;
    }
    else if(strcmp(argv[1],"add")==0){
        checkArgs(argc, 3);
        addFile(argv[2],argv[3]);
        return 0;
    }
    else if(strcmp(argv[1],"remove")==0){
        checkArgs(argc, 3);
        removeFile(argv[2],argv[3]);
        return 0;
    }

    portnum = getPort();
    mysocket = socket(AF_INET, SOCK_STREAM, 0);
    memset(&dest, 0, sizeof(dest)); /* zero the struct */
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = htonl(INADDR_ANY); /* set destination IP number*/
    dest.sin_port = htons(portnum); /* set destination port number*/

    int a = connect(mysocket, (struct sockaddr *)&dest, sizeof(struct sockaddr));
    check(a);

    if(strcmp(argv[1],"request")==0){
        int fd = open(argv[2], O_RDONLY);
        check(fd);
        char * files = readfile(fd);
        char * result = requestfiles(mysocket,files);
        free(files);
        close(fd);
    }
    else if(strcmp(argv[1],"send")==0){
        int fd = open(argv[2],O_RDONLY);
        check(fd);
        char * files = readfile(fd);
        sendfiles(mysocket,files);
        free(files);  
        close(fd);
    }
    else if(strcmp(argv[1],"create")==0){
        createProject(mysocket,argv[2]);
    }
    else if(strcmp(argv[1],"checkout")==0){
        checkout(mysocket,argv[2]);
    }
    else if(strcmp(argv[1],"currentversion")==0){
        currentversion(mysocket,argv[2]);
    }
    else if(strcmp(argv[1],"update")==0){
        updateProject(mysocket,argv[2]);
    }
    else if(strcmp(argv[1],"upgrade")==0){
        upgrade(mysocket,argv[2]);
    }
    else if(strcmp(argv[1],"commit")==0){
        commitProject(mysocket,argv[2]);
    }
    else if(strcmp(argv[1],"push")==0){
        pushProject(mysocket,argv[2]);
    }
    else if(strcmp(argv[1],"history")==0){
        history(mysocket,argv[2]);
    }
    else if(strcmp(argv[1],"destroy")==0){
        destroy(mysocket,argv[2]);
    }
    else if(strcmp(argv[1],"rollback")==0){
        checkArgs(argc, 3);
      rollback(mysocket,argv[2],argv[3]);
    }
    close(mysocket);
    return 0;
}
