#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include "lib.h"



/*
 * Recebe instrucoes pelo stdin, escreve num pipe
 * responsavel por enviar as instrucoes ao servidor
 * e de seguida espera pela resposta do servidor
 * pelo pipe responsavel pelas respostas deste cliente
 */

int main (int argc, char**argv){
  int pid=getpid();
  char fifo_name [MAX_BUFFER];
  int fdout=-1;

  sprintf(fifo_name,"/tmp/response_%d",pid);
  mkfifo(fifo_name,0666); // cria pipe de resposta

  char buffer [MAX_BUFFER];
  int rd;

  int fd=open("/tmp/instructions_cv",O_WRONLY); // abre pipe de instrucoes

  while (readln(0,buffer,MAX_BUFFER)>0){

    char *instruction = strtok(buffer,"\n");
    sprintf(instruction,"%s %d\n",buffer,pid);

    if (write(fd,instruction,strlen(instruction))<0) perror("writing"); // escreve instrucao para o servidor

    fdout=open(fifo_name,O_RDONLY);

    // espera pela resposta do servidor
    while ((rd=readln(fdout,buffer,MAX_BUFFER))>0){
      write(1,buffer,rd);
    }

    close(fdout);

  }

  close(fd);
  return 0;
}
