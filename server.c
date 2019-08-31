//
//  server.c
//  Server
//
//  Created by Lidor Zaken on 16/01/2019.
//  Copyright © 2019 Lidor Zaken. All rights reserved.
//

// Includes.
#include "threadpool.h"
#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

//Defines for ERRORS strings.
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define MAX_BUF 4000
#define USAGE "Command line usage: server <port> <pool-size> <max-number-of-request>"
#define ERROR "HTTP/1.0 %s\r\nServer: webserver/1.0\r\nDate: %s\r\n%sContent-Type: text/html\r\nContent-Length: %s\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>%s</TITLE></HEAD>\r\n<BODY><H4>%s</H4>\r\n%s\r\n</BODY></HTML>"
#define FILEDES "HTTP/1.0 %s\r\nServer: webserver/1.0\r\nDate: %s\r\nContent-Type: %s\r\nContent-Length: %s\r\nLast-Modified: %s\r\nConnection: close\r\n\r\n"
#define DIRCONT "<HTML>\r\n<HEAD><TITLE>Index of %s</TITLE></HEAD>\r\n<BODY>\r\n<H4>Index of %s</H4>\r\n<table CELLSPACING=8>\r\n<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\r\n"
#define DIRCONT2 "</table>\r\n<HR>\r\n<ADDRESS>webserver/1.0</ADDRESS>\r\n</BODY></HTML>"
#define TABLE "<tr><td><A HREF=\"%s\">%s</A></td><td>%s</td><td>%s</td></tr>\r\n"

//func decleration.
void error(char *msg);
char *get_mime_type(char *name);
int checkNumOfTokens(char* buf);
char* checkErrors(char* buf);
int  isPathOk(char* path);
void connectToServer(int size, char **argv);
char* readFromSocket(int fd);
int doWork(void* buffer);
void writeToSocket(int fd, char* msg);
char* getDirCon( char* path);
int confirmtionAccess(char * str);

//static vars.
bool isFile = false;
char path[MAX_BUF];

int main(int argc, char *argv[]) {
   if(argc!=4){
       printf(USAGE);
       exit(1);
   }
   connectToServer(argc, argv);
}

//error func
void error(char *msg){
    perror(msg);
    exit(1);
}

//this is the func that connect to the server.
void connectToServer(int size, char **argv){
   int sockfd;
   int* newSockfd = (int*)malloc(sizeof(int)*atoi(argv[3]));
   if(newSockfd==NULL){
        error("malloc error");
   }
   int portNum;
   int clilen;
   int n;
   int numOfReq = atoi(argv[3]);
   char buff[MAX_BUF];
   struct sockaddr_in serv_addr;
   struct sockaddr_in  cli_addr;
   threadpool* newThreadP = create_threadpool(atoi(argv[2]));


   sockfd = socket(PF_INET, SOCK_STREAM, 0); // socket decler.
   if(sockfd<0){
       error("Error socket opening");
   }

   portNum = atoi(argv[1]);
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
   serv_addr.sin_port = htons(portNum);
   if(bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))<0){
       error("Error on binding");
   }
   listen(sockfd, numOfReq); // listen open.
   clilen = sizeof(cli_addr); 


    for(int i =0; i<numOfReq ; i++){
       newSockfd[i] = accept(sockfd, (struct sockaddr*) &cli_addr, &clilen); // accept for all requests.
       if(newSockfd<0){
           perror("error on accept");
       }
       dispatch(newThreadP, doWork, &newSockfd[i]); // add new work to the list.
   }
   destroy_threadpool(newThreadP);
   free(newSockfd);
   close(sockfd);
}

char *get_mime_type(char *name) {
    char *ext = strrchr(name, '.');
    if (!ext) return NULL;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".au") == 0) return "audio/basic";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    return NULL;
}

// the func that check all errors and also what isnt error.
char* checkErrors(char* buf){
    char* arrayOfTokkens[3];
    char* word = (char*)malloc(MAX_BUF);
    if(word==NULL){
        error("malloc error");
    }
    memset(word,'\0',strlen(buf)+1);
    int numsOfTokens = checkNumOfTokens(buf);
    time_t now;
    char timebuf[128];
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
    bool isGet = false;
    char* newBuf = (char*)malloc(strlen(buf)+1);
    if(newBuf==NULL){
        error("malloc error");
    }
    strcpy(newBuf,buf);
    const char s[2] = " ";
    char *token;
    token = strtok(newBuf, s);
    int count = 0;
    while( token != NULL ) { //Inilazing the Tokens of the request.
        if(numsOfTokens<3){
            break;
        }
        arrayOfTokkens[count] = (char*)malloc(strlen(token)+1);
        if(arrayOfTokkens[count]==NULL){
            error("malloc error");
        }
        strcpy(arrayOfTokkens[count],token);
        token = strtok(NULL, s);
        count++;
   }
    if(numsOfTokens!=3){
        sprintf(word, ERROR, "400 Bad Request",timebuf,"","113","400 Bad Request","400 Bad Request","Bad Request.");
    }
    else if(strcmp(arrayOfTokkens[0],"GET")!=0){
        sprintf(word, ERROR, "501 Not supported",timebuf,"","129","501 Not supported","501 Not supported","Method is Not supported.");
    }
    else if(isPathOk(arrayOfTokkens[1])==-1){
        char* location = (char*)malloc(400);
        if(location==NULL){
           error("malloc error");
         }
        strcpy(location,"Location: ");
        strcat(location, arrayOfTokkens[1]);
        sprintf(word, ERROR, "302 Found",timebuf,strcat(location,"/\r\n"),"123","302 Found","302 Found","Directories must end with a slash.");
        free(location);
    }
    else if(isPathOk(arrayOfTokkens[1])==0){
        sprintf(word, ERROR, "404 Not Found",timebuf,"","112","404 Not Found","404 Not Found","File not found.");
    }
    else if(isPathOk(arrayOfTokkens[1])==1){
        char* indexFile = (char*)malloc(400);
        if(indexFile==NULL){
           error("malloc error");
         }
         struct stat sFile;
         stat(arrayOfTokkens[1]+1,&sFile);
         strcpy(indexFile,arrayOfTokkens[1]);
         strcat(indexFile,"index.html");
        if(access(indexFile+1,F_OK)!=-1){
            struct stat statFile;
            stat(indexFile,&statFile);
            char* fileCont = (char*)malloc(statFile.st_size+1);
            if(fileCont==NULL){
                 error("malloc error");
            }
            char lm[128];
            strftime(lm, sizeof(lm), RFC1123FMT, gmtime(&statFile.st_mtime));
            FILE *index;
            char c;
            char lenBuf[12];
            int i=1;
            index = fopen(indexFile+1,"r");
            c = fgetc(index);
            fileCont[0] = c;
            while(c!=EOF){
                c = fgetc(index);
                fileCont[i] = c;
                i++;
            }
            bzero(fileCont,'\0');
            fclose(index);
            sprintf(lenBuf,"%d",(int)strlen(fileCont));
            sprintf(word, FILEDES, "200 OK",timebuf,get_mime_type(indexFile),lenBuf,lm);
            strcat(word,fileCont);
            free(fileCont);
        }

        else if(S_ISDIR(sFile.st_mode)){
            struct stat statFile;
            char* dirc1 = (char*)malloc(MAX_BUF);
            if(dirc1==NULL){
                 error("malloc error");
            }
            char* dirc2 = (char*)malloc(MAX_BUF);
            if(dirc2==NULL){
                 error("malloc error");
            }
            char* tab = (char*)malloc(MAX_BUF);
            if(tab==NULL){
                 error("malloc error");
            }
            char lm[128];
            char sizeOfdirPage[12];
            int sOdP = strlen(getDirCon(arrayOfTokkens[1]+1))+strlen(DIRCONT)+strlen(DIRCONT2);
            sprintf(sizeOfdirPage,"%d",sOdP);
            strftime(lm, sizeof(lm), RFC1123FMT, gmtime(&statFile.st_mtime));
            stat(arrayOfTokkens[1],&statFile);
            sprintf(word, FILEDES, "200 OK",timebuf,"text/html",sizeOfdirPage,lm);
            sprintf(dirc1, DIRCONT,arrayOfTokkens[1],arrayOfTokkens[1]);
            strcat(word, dirc1);
            strcat(word,getDirCon(arrayOfTokkens[1]+1));
            strcat(word, DIRCONT2);
        }
        else{
            if(S_ISREG(sFile.st_mode)&&confirmtionAccess(arrayOfTokkens[1])==1){
                char lm[128];
                strftime(lm, sizeof(lm), RFC1123FMT, gmtime(&sFile.st_mtime));
                FILE *file;
                char c;
                isFile = true;
                char sizeOfFile[12];
                sprintf(sizeOfFile, "%ld", sFile.st_size);
                strcpy(word,"");
                sprintf(word, FILEDES, "200 OK",timebuf,get_mime_type(arrayOfTokkens[1]),sizeOfFile,lm);
                strcpy(path, arrayOfTokkens[1]);
            }
            else{
                sprintf(word, ERROR, "403 Forbidden",timebuf,"","129","403 Forbidden","403 Forbidden","Access denied.");
            }
        }
    }
    else{
        strcpy(word,"OKOKOK");
    }
    return word;
}

//func that help to find bad request.
int checkNumOfTokens(char* buf){
    char* newBuf = (char*)malloc(strlen(buf));
    if(newBuf==NULL){
        error("malloc error");
    }
    int count = 0;
    strcpy(newBuf, buf);
    const char s[3] = "\n";
    char *token;
    token = strtok(newBuf, s);
    int countSpaces = 0;
    int length = 0;
    int last = 0;
    length = (int)strlen(token);
    for (int i=0; i<length; i++) {
        if(token[i]==' '){
            if(last==0){
                countSpaces++;
            }
            else{
                last=1;
            }
        }
        else{
            last = 0;
        }
    }
    free(newBuf);
    return countSpaces+1;
}

// this func is check the path and provide to discover the error.
int isPathOk(char* path){
    // 1=ok 0=404 -1=302
    int len = strlen(path);
    struct stat statPath;
    if(strcmp(path,"/")==0){
        stat(path,&statPath);
        return 1;
    }
    else if(access(path+1,F_OK)!=-1){
        if(path[len-1]=='/'){
            return 1;
        }
        else{
            stat(path+1,&statPath);
            if(S_ISDIR(statPath.st_mode)&&path[len-1]!='/'){
                return -1;
            }
            else{
                return 1;
            }
        }
    }
    else{
        return 0;
    }
}

// this func is for the reading from socket.
char* readFromSocket(int fd){
    char* buf = (char*)malloc(MAX_BUF);
    if(buf==NULL){
        error("malloc error");
    }
    int n = read(fd, buf, MAX_BUF);
    if (n < 0){
        perror("ERROR reading from socket");
    }
    return buf;
}

// this func is doing a work if there is no error, or send error.
int doWork(void* socketFd){
    int fd = *((int*)socketFd);
    char* buf = readFromSocket(fd);
    char* isError = checkErrors(buf);
    if(strcmp(isError,"OKOKOK")!=0 && isFile==true){
        write(fd, isError,strlen(isError));
        struct stat sFile;
        stat(path+1,&sFile);
        FILE* file = fopen(path+1,"r");
        if(file==NULL){
            error("File error");
        }
        char* fileCont = (char*)malloc(sFile.st_size+1);
        if(fileCont==NULL){
            error("Malloc error");
            fclose(file);
        }
        memset(fileCont,'\0',sFile.st_size);
        int numRead = 0;
        while((numRead = fread(fileCont,1,sFile.st_size,file))>0){
            int n = write(fd,fileCont,sFile.st_size);
                if(n<0){
                    perror("ERROR writing to socket");
                }
        }

        fclose(file);
        free(fileCont);

    }
    else if(strcmp(isError,"OKOKOK")!=0){
        writeToSocket(fd, isError);
    }
    isFile = false;
    return 0;
}


// the writing to socket func.
void writeToSocket(int fd, char* msg){
    int n = write(fd,msg,strlen(msg));
    if(n<0){
        perror("ERROR writing to socket");
    }
    close(fd);
}

//func that gives the content of a dir.
char* getDirCon( char* path){ 
    char* dirContetnt = (char*)malloc(MAX_BUF);
    if(dirContetnt==NULL){
        error("malloc error");
    }
    struct dirent **namelist;
    int n;
    struct stat statPath;

    n = scandir(path, &namelist, NULL, alphasort);
    if (n == -1) {
        perror("scandir");
    }

    while (n--) {
        char* lineInTable = (char*)malloc(MAX_BUF);
        if(lineInTable==NULL){
            error("malloc error");
        }
        char* pathAndFile = (char*)malloc(MAX_BUF);
        if(pathAndFile==NULL){
            error("malloc error");
        }
        strcpy(pathAndFile,path);
        strcat(pathAndFile, namelist[n]->d_name);
        stat(pathAndFile, &statPath);
        char lm[128];
        strftime(lm, sizeof(lm), RFC1123FMT, gmtime(&statPath.st_mtime));
        char size[12];
        if(S_ISDIR(statPath.st_mode)){
            strcpy(size, "");
        }
        else{
            sprintf(size, "%ld", statPath.st_size);
        }
        sprintf(lineInTable ,TABLE ,namelist[n]->d_name, namelist[n]->d_name, lm, size);
        strcat(dirContetnt, lineInTable);
        free(lineInTable);
        free(pathAndFile);
        free(namelist[n]);
    }
    free(namelist);
    return dirContetnt;
}

// this func check if there are inconfirmations to a file or diractory.
int confirmtionAccess(char * str)
{
    char *token;
    char path[MAX_BUF]; 
    memset(path,'\0',MAX_BUF);
    char partOfPath[MAX_BUF];
    memset(partOfPath,'\0',MAX_BUF); 
    strcpy(path,str);
    struct stat sPath;
    stat(str, &sPath);    
    token = strtok(path, "/"); 
    while( token != NULL) 
    {
        strcat(partOfPath,token); 
        stat(partOfPath, &sPath); 
        if(S_ISDIR(sPath.st_mode) && !(sPath.st_mode &S_IXOTH)) 
            return 0;
        else if(!(sPath.st_mode & S_IROTH)) 
            return 0;
        token = strtok(NULL, "/"); 
        strcat(partOfPath,"/");
    }
    return 1;
}


