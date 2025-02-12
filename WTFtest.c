#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main () {
    char* buffer = (char*)malloc(200);

    //SETUP DIRECTORIES
    //Make server folder
    memset(buffer, 0, 200);
    sprintf(buffer, "mkdir server");
    system(buffer);

    //create user1 folder
    memset(buffer, 0, 200);
    sprintf(buffer, "mkdir user1");
    system(buffer);

    //create user2 folder
    memset(buffer, 0, 200);
    sprintf(buffer, "mkdir user2");
    system(buffer);


    //Copy WTF executable into the 2 user folders
    memset(buffer, 0, 200);
    sprintf(buffer, "cp WTF user1");
    system(buffer);

    memset(buffer, 0, 200);
    sprintf(buffer, "cp WTF user2");
    system(buffer);

    //copy server exec into server folder
    memset(buffer, 0, 200);
    sprintf(buffer, "cp ./WTFserver ./server");
    system(buffer);

    //Testing commands here
    
    //run server

    chdir("server");
    memset(buffer, 0, 200);
    sprintf(buffer, "./WTFserver 2343 &");
    system(buffer);
    
    //Enter user1 dir 
    if (chdir("../user1") != 0)   {
        perror("chdir() to user1 failed");
    }
    //config
    memset(buffer, 0, 200);
    sprintf(buffer, "./WTF configure 127.0.0.1 2343");
    system(buffer);

    memset(buffer, 0, 200);
    sprintf(buffer, "echo I will be creating project1 and project2 in user1 and destroying project2 and call create project1 again, which is an error\n\n");
    system(buffer);

    //User1: ./WTF create project1, returns success
    memset(buffer, 0, 200);
    sprintf(buffer, "./WTF create project1");
    system(buffer);

   
	
    //User1: ./WTF create project2, returns success
    memset(buffer, 0, 200);
    sprintf(buffer, "./WTF create project2");
    system(buffer);

    //User1: ./WTF create project1, returns error
    memset(buffer, 0, 200);
    sprintf(buffer, "./WTF create project1");
    system(buffer);

    
    //user1: ./WTF destroy project2
    memset(buffer, 0, 200);
    sprintf(buffer, "./WTF destroy project2");
    system(buffer);

    
    memset(buffer, 0, 200);
    sprintf(buffer, "echo I will create file1.txt and file2.txt in folder1 and calling add for both. Add for file3.txt should be an error\n");
    system(buffer);

    //User1: echo “file1” > ./project1/file1.txt
    memset(buffer, 0, 200);
    sprintf(buffer, "echo hello world! > ./project1/file1.txt");
    system(buffer);

    //mkdir “./client/project1 “folder1”
    memset(buffer, 0, 200);
    sprintf(buffer, "mkdir ./project1/folder1");
    system(buffer);

    //echo “file2” > ./client/project1/folder1/file2.txt
    memset(buffer, 0, 200);
    sprintf(buffer, "echo the quick brown fox > ./project1/folder1/file2.txt");
    system(buffer);

    //./WTF add project1 file1.txt
    memset(buffer, 0, 200);
    sprintf(buffer, "./WTF add project1 file1.txt");
    system(buffer);

    //./WTF add project1 folder1/file2.txt
    memset(buffer, 0, 200);
    sprintf(buffer, "./WTF add project1 folder1/file2.txt");
    system(buffer);


    //./WTF add project1 file3.txt
    memset(buffer, 0, 200);
    sprintf(buffer, "./WTF add project1 file3.txt");
    system(buffer);

    memset(buffer, 0, 200);
    sprintf(buffer, "echo I call commit project1 and tells us to add both file1.txt and file2.txt \n \n");
    system(buffer);
    
    
    //./WTF commit project1
    memset(buffer, 0, 200);
    sprintf(buffer, "./WTF commit project1");
    system(buffer);

    memset(buffer, 0, 200);
    sprintf(buffer, "echo I call push and it succeeds, adding both files to server version and making necessary changes to manifest\n");
    system(buffer);


    //./WTF push project1
    memset(buffer, 0, 200);
    sprintf(buffer, "./WTF push project1");
    system(buffer);

     memset(buffer, 0, 200);
    sprintf(buffer, "echo I go into user2 and call checkout\n");
    system(buffer);

    
    //Cd into User2
    if (chdir("../user2") != 0)   {
        perror("chdir() to user2 failed");
    }

    memset(buffer, 0, 200);
    sprintf(buffer, "./WTF configure 127.0.0.1 2343");
    system(buffer);

    
    //./WTF checkout project1
    memset(buffer, 0, 200);
    sprintf(buffer, "./WTF checkout project1");
    system(buffer);

     memset(buffer, 0, 200);
    sprintf(buffer, "echo I go into user1 again and remove file1.txt and change file2.txt in folder1\n");
    system(buffer);

    //Cd into User1
    if (chdir("../user1") != 0)   {
        perror("chdir() to user1 failed");
    }

    //echo “mod” > ./project1/folder1/file2.txt
    memset(buffer, 0, 200);
    sprintf(buffer, "echo new stuff >> ./project1/folder1/file2.txt");
    system(buffer);

    //./WTF remove project1 file1.txt
    memset(buffer, 0, 200);
    sprintf(buffer, "./WTF remove project1 file1.txt");
    system(buffer);

     memset(buffer, 0, 200);
    sprintf(buffer, "echo I call commit again and expect a D for file1.txt and M for file2.txt\n");
    system(buffer);
    //./WTF commit project1
    memset(buffer, 0, 200);
    sprintf(buffer, "./WTF commit project1");
    system(buffer);

     memset(buffer, 0, 200);
    sprintf(buffer, "echo I call push and it succeeds\n");
    system(buffer);
    //./WTF push project1
    memset(buffer, 0, 200);
    sprintf(buffer, "./WTF push project1");
    system(buffer);

     memset(buffer, 0, 200);
    sprintf(buffer, "echo I call history here\n");
    system(buffer);
    
    memset(buffer, 0, 200);
    sprintf(buffer, "./WTF history project1");
    system(buffer);

     memset(buffer, 0, 200);
    sprintf(buffer, "echo I call current version here\n");
    system(buffer);
    
    memset(buffer, 0, 200);
    sprintf(buffer, "./WTF currentversion project1");
    system(buffer);

    //Cd into User2
    if (chdir("../user2") != 0)   {
        perror("chdir() to user2 failed");
    }
    
    memset(buffer, 0, 200);
    sprintf(buffer, "echo I go to user 2 and call update\n");
    system(buffer);

    //./WTF update project1
    memset(buffer, 0, 200);
    sprintf(buffer, "./WTF update project1");
    system(buffer);

	//.WTF upgrade Project1
	memset(buffer,0,200);
	sprintf(buffer, "./WTF upgrade project1");
   	system(buffer);
    

    memset(buffer, 0, 200);
    sprintf(buffer, "echo I call rollback to version 1");
    system(buffer);

    memset(buffer,0,200);
    sprintf(buffer, "./WTF rollback project1 1");
    system(buffer);

    chdir("..");
    memset(buffer,0,200);
    sprintf(buffer, "killall server/WTFserver");
    system(buffer);
    
    free(buffer);
    return 1;
}
