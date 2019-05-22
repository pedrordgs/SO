#include <string.h>
#include "lib.h"



/*
 * Le uma linha de um descritor de ficheiro para um buffer
 */

ssize_t readln (int fildes, void *buf, size_t nbyte){
  char c;
  int i;
  for (i=0;i<nbyte && read(fildes,&c,1)>0;i++){
    memcpy(buf+i,&c,1);
    if (c=='\n'){ // para ao encontrar o '\n'
      i++;
      break;
    }
  }
  return i;
}
