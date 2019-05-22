#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include "lib.h"




/*
 *  Le e retorna o numero de vendas que ja foram agreagadas anteriormente
 *  Ficheiro vendas contem essa informação na posicao 0
 */

int getAgregadas(){

  int fd = open("vendas",O_RDONLY);

  int vendas_agregadas;
  read(fd,&vendas_agregadas,sizeof(int));

  close(fd);
  return vendas_agregadas;
}








/*
 *  Atualiza o numero de vendas que ja foram agregadas no ficheiro vendas
 * que guarda esse valor na posicao 0
 */


int atualizaAgregadas(int vendas_agregadas){

  int fd=open("vendas",O_WRONLY);
  write(fd,&vendas_agregadas,sizeof(int));

  close(fd);
  return 0;
}









/*
 *  Insere o artigo num array com o numero de artigos mais vendidos
 * ordenados pela quantidade vendida
 */

int insereOrd (Venda *v, Venda* arr[]){
  int i,j;

  //Procura onde colocar o artigo
  for(i=0;i<NUMERO_MAIS_VENDIDOS;i++)
    if (arr[i]->quant < v->quant) break;

  /*
   * Move todos os outros artigos para a direita para reservar
   * espaco para que o novo seja colocado
   */
  for (j=NUMERO_MAIS_VENDIDOS-1;j<i;j++)
    arr[j]=arr[j-1];

  Venda *a = malloc(sizeof(struct venda));
  a->cod=v->cod;
  a->fat=v->fat;
  a->quant=v->quant;
  arr[i]=a; // escreve o artigo

  return 0;
}








/*
 * Inicializa o array de mais vendidos com tudo a zeros
 */

int inicializaVendidos(Venda* arr []){
  int i;
  for (i=0;i<NUMERO_MAIS_VENDIDOS;i++){
    Venda *v = malloc(sizeof(struct venda));
    v->cod=0;
    v->quant=0;
    v->fat=0;
    arr[i]=v;
  }
  return 0;
}






/*
 * Escreve o array de artigos mais vendidos num ficheiro auxiliar
 * criado na pasta ./help para que seja posteriormente lido pelo servidor
 */

int escreveMaisVendidos(Venda* arr []){
  int i;
  int fd = open("./help/maisvendidos",O_CREAT | O_WRONLY,0666);
  for (i=0;i<NUMERO_MAIS_VENDIDOS;i++){
    if(arr[i]->cod!=0){
      write(fd,&arr[i]->cod,sizeof(int)); //escreve cada codigo de artigo dos mais vendidos
    }
  }
  return 0;
}






/*
 * Envia um sinal para o servidor, para que este saiba que foram
 * atualizados os artigos mais vendidos
 */

int sendSignalSV (){
  int fda = open("artigos",O_RDONLY);
  int pid_sv;
  read(fda,&pid_sv,sizeof(int)); // le o pid do servidor no ficheiro artigos
  if (pid_sv!=0) kill(pid_sv,SIGUSR1);
  close(fda);
  return 0;
}






/*
 * Calcula o numero de artigos, recorrendo-se do ficheiro artigos
 */

int nr_artigos (){
  int fda = open("artigos",O_RDONLY);
  lseek(fda,sizeof(int),SEEK_SET);

  int artigos = lseek(fda,0,SEEK_END)/sizeof(struct artigo);

  close(fda);
  return artigos;
}






/*
 * Calcula o numero de forks que o agregador tem que dar de forma
 * a que seja possivel fazer a agregacao completa sem qualquer falha
 * e que haja um aproveitamento da concorrencia
 */

int numero_forks(int nr_vendas){
  int i=nr_vendas; // recebe o numero de vendas que serao necessarias agregar
  int artigos=nr_artigos(); // calcula numero de artigos
  /*
   * Procura numero ideal de tal forma a que o ficheiro seja distribuido de
   * igual para igual em cada processo e que seja impossivel o processo acabar
   * sem realizar qualquer agregacao
   */
  while(i>1 && (nr_vendas%i!=0 || nr_vendas/i<artigos+1)) //
    i--;
  return i;
}







/*
 * Converte uma string recebida como argumento, numa struct venda
 */

Venda* string2Vend (char* linha_venda){
  strtok(linha_venda,"\n");

  char* campos [ARGS_VENDA];
  char*token;
  int i=0;

  token=strtok(linha_venda," ");
  while(token!=NULL){
    campos[i++]=token;
    token=strtok(NULL," ");
  }

  if (i<3) return NULL;

  Venda *v = malloc(sizeof(struct venda));
  v->cod=atoi(campos[0]);
  v->quant=atoi(campos[1]);
  v->fat=atof(campos[2]);

  return v;
}







/*
 * Converte as linhas de vendas recebidas pelo stdin e cria um
 * ficheiro em que as guarda numa struct venda
 */

int convertStdIn2File (int argc){

  char buffer [MAX_BUFFER];
  int vendas_agregadas=getAgregadas();

  //no caso de correr o agregador pelo ma, temos de ver quantas vendas ja foram agregadas
  if (argc>1){
    lseek(0,sizeof(int),SEEK_SET);
    for(int c=0;c<vendas_agregadas;c++)
      readln(0,buffer,MAX_BUFFER);
  }

  int nr_vendas=0;

  int fd=open("ag_venda",O_CREAT | O_WRONLY | O_APPEND, 0666);

  while(readln(0,buffer,MAX_BUFFER)>0){
    Venda *v = malloc(sizeof(struct venda));
    v=string2Vend(buffer); // converte string numa struct venda

    write(fd,v,sizeof(struct venda));
    nr_vendas++;
    vendas_agregadas++;
  }

  if (argc>1) atualizaAgregadas(vendas_agregadas); //atualizar o numero de vendas agregadas

  close(fd);
  return nr_vendas; // devolve o numero de vendas que serao agregadas
}








/*
 * Agrega as vendas de cada processo, criando um ficheiro para o
 * respetivo processo e guardando la os resultados da agregacao
 */

int agregaVendas(int f, int nr_vendas_fork){
  int fdv = open("ag_venda",O_RDONLY);
  char name [MAX_BUFFER];
  sprintf(name,"ag_aux%d",f); //cria ficheiro correspondente ao processo

  int fda = open(name,O_CREAT | O_RDWR,0666);
   /*
    * Inicializa ficheiro auxiliar do respetivo filho, com todos os artigos
    * iniciados a 0, para que o acesso, mais tarde, seja direto
    */
  int artigos=nr_artigos();
  for(int a=0;a<artigos;a++){
    Venda *v = malloc(sizeof(struct venda));
    v->cod=a+1;
    v->quant=0;
    v->fat=0;

    write(fda,v,sizeof(struct venda));
  }

  int offset = f * nr_vendas_fork * sizeof(struct venda); //calcula onde o processo ira comecar a agregar
  lseek(fdv,offset,SEEK_SET);
  int vendas_lidas=0;

  Venda *v = malloc(sizeof(struct venda));
  while(vendas_lidas<nr_vendas_fork && read(fdv,v,sizeof(struct venda))>0){

    int cod=v->cod;
    lseek(fda,(cod-1)*sizeof(struct venda),SEEK_SET);
    Venda *aux = malloc(sizeof(struct venda));
    read(fda,aux,sizeof(struct venda));
    aux->quant+=v->quant; // soma as quantidades
    aux->fat+=v->fat; // soma o valor faturado
    lseek(fda,-sizeof(struct venda),SEEK_CUR);
    write(fda,aux,sizeof(struct venda));
    vendas_lidas++;
  }

  close(fdv);
  close(fda);
  return 0;
}








/*
 * Junta todos os ficheiros criados anteriormente
 * para que possa ser novamente agregado, caso seja necessario
 */

int juntaAg(int nr_forks){
  int fd = open("agAUX",O_WRONLY | O_CREAT | O_APPEND,0666);
  int i,r=0;
  char name [MAX_BUFFER];
  // para ficheiro ag_aux(numero do filho) vai copiar para um ficheiro auxiliar todas as vendas agregadas
  for(i=0;i<nr_forks;i++){
    sprintf(name,"ag_aux%d",i);
    int fdaux=open(name,O_RDONLY);

    Venda *v = malloc(sizeof(struct venda));
    while(read(fdaux,v,sizeof(struct venda))>0){
      if (v->quant!=0){
        write(fd,v,sizeof(struct venda));
        r++;
      }
    }

    close(fdaux);
    unlink(name);
  }

  unlink("ag_venda");
  rename("agAUX","ag_venda");
  close(fd);
  return r;
}







/*
 * Converte as vendas agregadas, em strings para que sejam
 * legiveis e imprime no stdout os resultados da agregacao
 */

int printAgregadas(){

  Venda* maisVendidos [NUMERO_MAIS_VENDIDOS];
  inicializaVendidos(maisVendidos); // inicia um array com os mais vendidos

  int fd = open("ag_venda",O_RDONLY);
  char linha_agregada [MAX_BUFFER];

  Venda *v = malloc(sizeof(struct venda));

  while(read(fd,v,sizeof(struct venda))>0){

    if (v->quant!=0){

      insereOrd(v,maisVendidos); // vai inserindo ordenadamente cada artigo no array mais vendidos

      sprintf(linha_agregada,"%d %d %f\n",v->cod,v->quant,v->fat);
      write(1,linha_agregada,strlen(linha_agregada));

    }
  }

  escreveMaisVendidos(maisVendidos); // atualiza os mais vendidos no ficheiro

  close(fd);
  unlink("ag_venda"); //elimina ficheiro auxiliar
  return 0;
}








/*
 * Decide quantos filhos e quantas vendas cada filho tem de tratar.
 * Cria os filhos e espera ate que eles agreguem.
 * Depois junta os resultados de cada filho
 */

int ag (int nr_vendas){
  int nr_forks = numero_forks(nr_vendas);

  int nr_vendas_fork = nr_vendas / nr_forks;

  int i=0,status;

  while (i<nr_forks){

    if (fork()==0){
      agregaVendas(i,nr_vendas_fork); // cada filho agrega a sua parte
      exit(0);
    }

    else
      i++;

  }
  while(wait(&status)>0); // espera por todos os filhos

  return (nr_vendas-juntaAg(nr_forks)); // retorna o numero de mudancas que houve na agregacao
}






/*
 * Trata de juntar as pecas e chamar cada funcao
 * de modo a tornar possivel a agregacao
 */

int main(int argc, char** argv){

  int nr_vendas = convertStdIn2File(argc);
  int mud=ag(nr_vendas);

  //verifica se ja esta tudo agregado
  while(mud>0){
    nr_vendas=nr_vendas-mud;
    mud=ag(nr_vendas);
  }

  printAgregadas(); // imprime os resultados no stdout

  sendSignalSV(); // envia sinal ao servidor

  return 0;
}
