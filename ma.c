#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <stdio.h>
#include <signal.h>
#include "lib.h"




/*
 * Verifica se o ficheiro strings precisa de limpeza
 */

int need_clean (){

  int tam_strings=0;
  int tam_total;

  int fda=open("artigos",O_RDONLY);
  int fds=open("strings",O_RDONLY);

  lseek(fda,sizeof(int),SEEK_SET); // necessario pois a primeira coisa guardada no artigos e o pid do servidor

  tam_total=lseek(fds,0,SEEK_END); // calcula tamanho total de strings

  Art *a = malloc(sizeof(struct artigo));

  // calcula o tamanho util de strings
  while(read(fda,a,sizeof(struct artigo))){
    tam_strings+=a->tam_str;
  }

  return tam_strings/tam_total<=0.8; // se o tamanho do lixo atingir 20% precisa de limpar
}





/*
 * Limpa o lixo do ficheiro strings
 */

int clean_strings (){

  int fda = open ("artigos",O_RDWR);
  int fds = open ("strings",O_RDONLY);
  int fdns = open ("new_strings",O_CREAT | O_WRONLY,0666); // cria novo ficheiro auxiliar

  Art *a = malloc(sizeof(struct artigo));

  lseek(fda,sizeof(int),SEEK_SET);

  // copia todas as strings usadas para o ficheiro auxiliar
  while(read(fda,a,sizeof(struct artigo))){

    lseek(fds,a->string_nr,SEEK_SET);

    char* buffer [a->tam_str];
    if (read(fds,buffer,a->tam_str)<0) perror("reading string");

    int pos=lseek(fdns,0,SEEK_END);
    write(fdns,buffer,a->tam_str);

    a->string_nr=pos;
    lseek(fda,-sizeof(struct artigo),SEEK_CUR);
    write(fda,a,sizeof(struct artigo));

  }

  unlink("strings"); // remove ficheiro antigo
  rename("new_strings","strings"); // altera o nome do ficheiro novo

  close(fda);
  close(fds);
  close(fdns);
  return 0;
}







/*
 * Insere um artigo no ficheiro artigos e consequentemente
 * a sua string no ficheiro strings
 */

int insere(char** argv){

  int fda = open ("artigos",O_CREAT | O_RDWR,0666);
  int fds = open ("strings",O_CREAT | O_WRONLY | O_APPEND,0666);
  int pid_sv=0;

  int pos=lseek(fda,0,SEEK_END);
  int codigo = lseek(fds,0,SEEK_END);

  // Inicia o pid do servidor a 0 caso o artigos tenha sido criado agora
  if (pos==0){
    pid_sv=0;
    write(fda,&pid_sv,sizeof(int));
  }

  lseek(fda,0,SEEK_SET);
  if(read(fda,&pid_sv,sizeof(int))<=0) perror("reading"); // le o pid do servidor
  lseek(fda,0,SEEK_END); // escreve artigo no final do ficheiro

  Art *new = malloc(sizeof(struct artigo));
  new->string_nr=codigo;
  new->preco=atof(argv[1]);
  new->tam_str=strlen(argv[0]);

  write(fda,new,sizeof(struct artigo));
  write(fds,argv[0],strlen(argv[0]));

  int id = pos/sizeof(struct artigo);
  char show_cod [MAX_BUFFER];
  sprintf(show_cod,"Codigo: %d\n",id+1);
  write(1,show_cod,strlen(show_cod)); // escreve no pipe de resposta a resposta para enviar ao cliente

  /*
   * Se o ficheiro nao tiver sido criado agora, envia sinal ao servidor avisando que
   * foram adicionados artigos e ele deve iniciar o respetivo stock
   */
  if (pid_sv!=0) kill(pid_sv,SIGUSR1); //

  free(new);
  close(fda);
  close(fds);

  return 0;
}






/*
 * Altera o nome de um artigo
 */

int altera_nome (char** argv){

  int fda = open ("artigos",O_RDWR);
  int fds = open ("strings",O_WRONLY | O_APPEND);


  int offset = (atoi(argv[0])-1)*(sizeof(struct artigo)); // procura o artigo
  if (offset>=lseek(fda,0,SEEK_END)) return 0; // se nao existir, acaba a funcao

  lseek(fda,sizeof(int),SEEK_SET);
  lseek(fda,offset,SEEK_CUR);
  int codigo=lseek(fds,0,SEEK_END); // guarda a posicao da nova string

  Art *update = malloc(sizeof(struct artigo));
  read(fda,update,sizeof(struct artigo));

  // Atualiza os dados da nova string no artigo
  update->string_nr=codigo;
  update->tam_str=strlen(argv[1]);

  lseek(fda,-sizeof(struct artigo),SEEK_CUR);
  write(fda,update,sizeof(struct artigo)); // reescreve o artigo no lugar em que estava
  write(fds,argv[1],strlen(argv[1])); // escreve a nova string

  if (need_clean()) clean_strings(); // verifica se, depois da insercao da nova string, sera necessario limpeza


  free(update);
  close(fda);
  close(fds);

  return 0;
}









/*
 * Altera o preco de um artigo
 */

int altera_preco (char** argv){
  int pid_sv;

  int fda = open ("artigos",O_RDWR);

  int offset = (atoi(argv[0])-1)*(sizeof(struct artigo)); // procura o artigo
  if (offset>=lseek(fda,1,SEEK_END)) return 0; // verifica se o artigo existe

  lseek(fda,0,SEEK_SET);
  read(fda,&pid_sv,sizeof(int));
  lseek(fda,offset,SEEK_CUR);


  Art *update = malloc(sizeof(struct artigo));
  read(fda,update,sizeof(struct artigo));

  update->preco=atof(argv[1]); // altera o preco do artigo

  lseek(fda,-sizeof(struct artigo),SEEK_CUR);
  write(fda,update,sizeof(struct artigo)); // reescreve o artigo no devido lugar

  if (pid_sv!=0) kill(pid_sv,SIGUSR1); // envia sinal ao servidor avisando que foi alterado o preco de um artigo

  free (update);
  close(fda);

  return 0;
}






/*
 * Le o pid do servidor
 */

int getpidsv (){
  int pid_sv;
  int fda=open("artigos",O_RDONLY); // pid escrito na posicao 0 do artigos
  read(fda,&pid_sv,sizeof(int));
  close(fda);
  return pid_sv;
}







/*
 * Recebe as instrucoes do stdin e chama a funcao requisitada
 */

int main(int argc,char**argv){
  char buffer [MAX_BUFFER];
  int pid_sv=getpidsv(); // le o pid do servidor

  while(readln(0,buffer,MAX_BUFFER)>0){

    strtok(buffer,"\n");

    // Divide a instrucao em diferentes argumentos
    int i=0;
    char* args [ARGS_NUM];
    char *token = strtok(buffer," ");
    token=strtok(NULL," ");

    while(token!=NULL && i<ARGS_NUM){
      args[i]=token;
      token=strtok(NULL," ");
      i++;
    }

    switch(buffer[0]){

      case 'i': // Insere
        insere(args);
        break;

      case 'n': // Muda nome
        altera_nome(args);
        break;

      case 'p': // Muda preco
        altera_preco(args);
        break;

      case 'a': // Agregador
        kill(pid_sv,SIGUSR2); // envia sinal ao servidor para correr o agregador
        break;

      default:
        perror("invalid_request");
    }

  }
  return 0;
}
