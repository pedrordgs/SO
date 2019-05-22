#include <unistd.h>

#define MAX_BUFFER 1024
#define NUMERO_MAIS_VENDIDOS 100 // define o tamanho do array de artigos mais vendidos
#define ARGS_VENDA 3 // numero de argumentos de uma venda
#define ARGS_NUM 2 // numero de argumentos do ma
#define ARGS_MAX_NUM 3 // numero maximo de argumentos do cliente


typedef struct venda{
  int cod; // codigo artigos
  int quant; // quantidade vendida
  double fat; // total faturado
}Venda;


typedef struct artigo{
  int string_nr; // posicao da string no ficheiro strings.txt
  double preco; // preco do artigo
  int tam_str; // tamanho da string correspondente
}Art;

ssize_t readln(int fildes, void *buf, size_t nbyte); // leitura de uma linha para um buffer
