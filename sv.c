#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include "lib.h"




Venda* maisVendidos [NUMERO_MAIS_VENDIDOS]; // declara array dos artigos mais vendidos





/*
 * Calcula o numero de artigos
 */

int nr_artigos (){
  int fda = open("artigos",O_RDONLY);
  lseek(fda,sizeof(int),SEEK_SET); // posicao 0 contem o pid do servidor

  int artigos = lseek(fda,0,SEEK_END)/sizeof(struct artigo);

  close(fda);
  return artigos;
}








/*
 * Carrega os artigos mais vendidos para o array maisVendidos
 */

int carregaArtigos (){
  int pid=getpid();
  int cod;

  int fda = open("artigos",O_RDWR | O_CREAT, 0666);
  lseek(fda,0,SEEK_SET);
  write(fda,&pid,sizeof(int)); // escreve o seu pid na posicao 0 do ficheiro artigos

  int fdaux = open("./help/maisvendidos",O_RDONLY); // abre o ficheiro auxiliar com os artigos mais vendidos

  int i=0,help;

  if (fdaux>0){
    // Le todos os artigos mais vendidos escritos no ficheiro auxiliar e adiciona no array
    Art *art = malloc(sizeof (struct artigo));
    while(read(fdaux,&help,sizeof(int))){

      if (i<NUMERO_MAIS_VENDIDOS){
        lseek(fda,sizeof(int)+(help-1)*sizeof(struct artigo),SEEK_SET);
        read(fda,art,sizeof(struct artigo));
        Venda *v = malloc(sizeof(struct venda));
        v->cod=help;
        v->fat=art->preco;
        maisVendidos[i]=v;
      }

      i++;
    }
    free(art);
  }
  /*
   * Inicializa o stock de todos os artigos que
   * ainda nao deram entrada nem saida de stock
   */
  int artigos = nr_artigos();

  int fds=open("stock",O_CREAT | O_RDWR,0666);

  for (cod=1;cod<=artigos;cod++){
    int offset = (cod-1)*(sizeof(int));

    if (offset>=lseek(fds,0,SEEK_END)){ // se o artigo ainda nao tiver dado entrada nem saida
      int new_stock;
      new_stock=0;
      write(fds,&new_stock,sizeof(int)); // inicia artigo com 0 de stock
    }
  }

  close(fda);
  return 0;
}








/*
 * Handler do SIGUSR1 que carrega os artigos mal receba o sinal
 */
void fst_sig_rec (int signum){
  carregaArtigos();
}






/*
 * Handler do SIGUSR2 que corre o agregador mal receba o sinal
 */
void sigusr_handler (int signum){
  if (fork()==0){ // cria um filho para que seja possivel continuar a usar o servidor enquanto corre o agregador

    time_t now;
    time(&now);
    struct tm *tnow = localtime(&now);
    int hours=tnow->tm_hour;
    int minutes=tnow->tm_min;
    int seconds=tnow->tm_sec;
    int day=tnow->tm_mday;
    int month=tnow->tm_mon+1;
    int year=tnow->tm_year+1900;

    char fich_name [MAX_BUFFER];
    sprintf(fich_name,"./agregacoes/%d-%02d-%02dT%02d:%02d:%02d.txt",year,month,day,hours,minutes,seconds); // cria ficheiro com data e hora a que ocorreu a agregacao

    int fdin=open("vendas",O_RDONLY);
    int fdout=open(fich_name,O_CREAT | O_WRONLY,0666);

    dup2(fdin,0); // redireciona o vendas para o stdin do agregador
    dup2(fdout,1); // redireciona o ficheiro com data e hora para stdout do agregador

    close(fdin);
    close(fdout);

    execl("ag","ag","1",NULL); // executa o agregador com argumentos para que este saiba que foi executado a pedido do ma
  }
}











/*
 * Procura o preco de um artigo
 */

double getPreco (int cod){
  double preco;
  int i;

  // Primeiramente procura no array maisVendidos
  for(i=0;i<NUMERO_MAIS_VENDIDOS && maisVendidos[i]!=NULL;i++){
    if (maisVendidos[i]->cod==cod){
      preco=maisVendidos[i]->fat;
      break;
    }
  }

  // Se nao encontrar, vai ao ficheiro artigos buscar o preco
  if (maisVendidos[i]==NULL || i==NUMERO_MAIS_VENDIDOS){

    int fdp = open("artigos",O_RDONLY);

    int offset = sizeof(int)+sizeof(struct artigo)*(cod-1);
    lseek(fdp,offset,SEEK_SET);

    Art *art = malloc(sizeof(struct artigo));
    read(fdp,art,sizeof(struct artigo));

    preco=art->preco;
    free(art);
    close(fdp);
  }

  return preco;
}











/*
 * Escreve uma venda no ficheiro vendas
 */

int registaVenda (int cod, int quant){

  int fdv=open("vendas",O_CREAT | O_WRONLY,0666);
  int vendas_agregadas=0;
  if(lseek(fdv,0,SEEK_END)==0) // caso tenha sido criado o ficheiro agora, inicia o numero de vendas ja agregadas como 0
    write(fdv,&vendas_agregadas,sizeof(int));


  char venda [MAX_BUFFER];

  double preco=getPreco(cod); // procura o preco do artigo

  sprintf(venda,"%d %d %f\n",cod,quant,preco*quant); // cria string com o formato de venda "COD QUANT FAT"

  write(fdv,venda,strlen(venda)); // escreve a venda no ficheiro

  close(fdv);

  return 0;
}









/*
 * Responde ao cliente a quantidade de stock do artigo que ele requisitou
 */

int getStock(char** argv){
  // recebe como argumentos o codigo do artigo e o pid do cliente
  int pid =atoi(argv[1]);
  int cod =atoi(argv[0]);

  char response_fifo [MAX_BUFFER];
  sprintf(response_fifo,"/tmp/response_%d",pid);

  int fdr=open(response_fifo,O_WRONLY); // abre o pipe de resposta

  int fds=open("stock",O_RDONLY);

  if (cod>nr_artigos()){ // verifica se o artigo existe
    char *err;
    err=strdup("Artigo nao existe\n");

    write(fdr,err,strlen(err));

    close(fdr);
    close(fds);
    return 0;
  }

  int offset = (atoi(argv[0])-1)*(sizeof(int));
  if (offset>=lseek(fds,0,SEEK_END)) return 0;

  lseek(fds,offset,SEEK_SET);

  int stock;
  read(fds,&stock,sizeof(int));

  char buffer [MAX_BUFFER];
  sprintf(buffer,"%d %f\n",stock,getPreco(cod)); // responde com quantidade de stock e preco do artigo
  write(fdr,buffer,strlen(buffer));

  close(fds);
  close(fdr);
  return 0;
}









/*
 * Atualiza o stock de um artigo a pedido do cliente, respondendo com a quantidade de stock
 */

int atualizaStock(char** argv){
  // recebe como argumentos o codigo do artigo, a quantidade que entrou ou saiu e o pid do cliente
  int cod = atoi(argv[0]);
  int quant = atoi(argv[1]);
  int pid = atoi(argv[2]);

  char response_fifo [MAX_BUFFER];
  sprintf(response_fifo,"/tmp/response_%d",pid);

  int fdr=open(response_fifo,O_WRONLY); // abre pipe de resposta
  int fds=open("stock",O_CREAT | O_RDWR,0666);

  if (cod>nr_artigos()){ // verifica se artigo existe
    char *err;
    err=strdup("Artigo nao existe\n");

    write(fdr,err,strlen(err));

    close(fdr);
    close(fds);
    return 0;
  }

  int offset = (cod-1)*(sizeof(int));

  lseek(fds,offset,SEEK_SET);

  int updt_stock;
  read(fds,&updt_stock,sizeof(int));

  updt_stock+=quant; // atualiza a quantidade de stock

  lseek(fds,-sizeof(int),SEEK_CUR);
  write(fds,&updt_stock,sizeof(int)); // reescreve a quantidade no local certo

  char buffer [MAX_BUFFER];
  sprintf(buffer,"%d\n",updt_stock);
  write(fdr,buffer,strlen(buffer)); // envia para o pipe de resposta, a nova quantidade de stock

  if (quant<0) registaVenda(cod,abs(quant)); // no caso de haver saidas de stock, e registada a venda

  close(fds);
  close(fdr);
  return 0;
}









/*
 * Le todas as instrucoes recebidas pelo pipe de instrucoes,
 * separa os argumentos de cada instrucao e depois chama a
 * funcao conforme a requisicao do cliente
 */

int main (int argc, char** argv){
  char buffer [MAX_BUFFER];
  char* err;
  carregaArtigos(); // carrega os artigos para a cache

  // Configura os sinais SIGUSR1 e SIGUSR2
  signal(SIGUSR1,fst_sig_rec);
  signal(SIGUSR2,sigusr_handler);

  mkfifo("/tmp/instructions_cv",0666); // cria o pipe de instrucoes

  err=strdup("invalid request\n");

  while (1){

    int fd=open("/tmp/instructions_cv",O_RDONLY);

    while(readln(fd,buffer,MAX_BUFFER)>0){

      // separa os argumentos
      int i=0;
      char* args [ARGS_MAX_NUM];
      char *token = strtok(buffer,"\n");
      token = strtok(buffer," ");

      while(token!=NULL && i<ARGS_MAX_NUM){
        args[i]=token;
        token=strtok(NULL," ");
        i++;
      }

      switch(i){

        case 2: // mostrar o stock, argumentos [COD,PID]
          getStock(args);
          break;

        case 3: // atualizar o stock, argumentos [COD,QUANT,PID]
          atualizaStock(args);
          break;

        default:
          write(2,err,strlen(err));
      }

    }

    close(fd);

  }

  free(err);
  return 0;
}
