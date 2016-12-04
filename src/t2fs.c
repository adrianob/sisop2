#include "../include/t2fs.h"
#include "../include/apidisk.h"
#include "../include/bitmap2.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

typedef enum { false, true } bool;
struct t2fs_record * get_record_from_path(char *pathname);
bool in_root_path(char *pathname);
char * get_last_name(char *pathname);
struct t2fs_record * get_last_record(char *pathname);
int get_handle();

static int initialized = false;

typedef struct {
  struct t2fs_record record;
  bool occupied;
  unsigned int offset;
} OPEN_RECORD;

int MAX_RECORDS = 20;
OPEN_RECORD open_records[20];
struct t2fs_superbloco superbloco;
unsigned int first_inode_block;
unsigned int first_data_block;
unsigned int block_size;
unsigned int record_size = 64;
unsigned int inode_size = 16;

static void init()
{
  if(initialized){
    return;
  }else {
    initialized = true;
  }
  char *superblock_buffer = malloc(SECTOR_SIZE);
  if(!superblock_buffer){
   printf("falha malloc");
   return;
  }
  int return_code;
  return_code = read_sector(0, (unsigned char *)superblock_buffer);
  if(return_code != 0){
     printf("erro na leitura");
  }
  //fill superblock struct
  memcpy(&superbloco.id, superblock_buffer, sizeof(superbloco.id));
  memcpy(&superbloco.version, superblock_buffer + 4, sizeof(superbloco.version));
  memcpy(&superbloco.superblockSize, superblock_buffer + 6, sizeof(superbloco.superblockSize));
  memcpy(&superbloco.freeBlocksBitmapSize, superblock_buffer + 8, sizeof(superbloco.freeBlocksBitmapSize));
  memcpy(&superbloco.freeInodeBitmapSize, superblock_buffer + 10, sizeof(superbloco.freeInodeBitmapSize));
  memcpy(&superbloco.inodeAreaSize, superblock_buffer + 12, sizeof(superbloco.inodeAreaSize));
  memcpy(&superbloco.blockSize, superblock_buffer + 14, sizeof(superbloco.blockSize));
  memcpy(&superbloco.diskSize, superblock_buffer + 16, sizeof(superbloco.diskSize));

  first_inode_block = superbloco.superblockSize + 
		      superbloco.freeInodeBitmapSize +
                      superbloco.freeBlocksBitmapSize;
  first_data_block = first_inode_block + superbloco.inodeAreaSize;
  block_size = superbloco.blockSize;

  return_code = read_sector(1, (unsigned char *)superblock_buffer);
  if(return_code != 0){
     printf("erro na leitura");
  }
  //printf("%.*s\n", 4, superbloco.id);
  //printf("%02x\n", superbloco.version);
  //printf("%d\n", getBitmap2(BITMAP_DADOS, 10));
}

int get_handle(){
  int i;
  int handle = -1;
  OPEN_RECORD handle_open_record;
  for(i=0; i<MAX_RECORDS;i++){
    handle_open_record = open_records[i];
    if(!handle_open_record.occupied){
      handle = i;
      break;
    }
  }
  return handle;
}

struct t2fs_record * get_last_record(char *pathname){
  char *name = malloc(sizeof(char) * 256 );
  strcpy(name, pathname);

  char *current_path;
  char *last_path = malloc(sizeof(char) * 256 ); 
  char *temp_path = malloc(sizeof(char) * 256 ); 
  const char separator[2] = "/";
  
  /* get the first token */
  current_path = strtok(name, separator);
  
  /* walk through other tokens */
  while( current_path != NULL ) 
  {
    strcpy(temp_path, current_path);
    current_path = strtok(NULL, separator);
    if(current_path != NULL){
      strcat(last_path, "/");
      strcat(last_path, temp_path);
    }
  }
  return get_record_from_path(last_path);
}

char * get_last_name(char *pathname){
  char *name = malloc(sizeof(char) * 256 );
  strcpy(name, pathname);

  char *current_path;
  char *last_path = malloc(sizeof(char) * 256 ); 
  const char separator[2] = "/";
  
  /* get the first token */
  current_path = strtok(name, separator);
  
  /* walk through other tokens */
  while( current_path != NULL ) 
  {
    strcpy(last_path, current_path);
    current_path = strtok(NULL, separator);
  }
  return last_path;
}

bool in_root_path(char *pathname){
  char *name = malloc(sizeof(char) * 256 );
  strcpy(name, pathname);

  char *current_path;
  const char separator[2] = "/";
  int i = 0;
  
  /* get the first token */
  current_path = strtok(name, separator);
  
  /* walk through other tokens */
  while( current_path != NULL ) 
  {
    i++;
    current_path = strtok(NULL, separator);
  }
  return i == 1;
}

struct t2fs_record * get_record_from_path(char *pathname){
  //read first root inode
  struct t2fs_inode *inode = malloc(sizeof(struct t2fs_inode));
  unsigned char *buffer = malloc(SECTOR_SIZE);
  read_sector(first_inode_block, buffer);
  memcpy(inode, buffer, sizeof(struct t2fs_inode));

  int offset = 0;
  char *current_path;
  const char separator[2] = "/";
  int sector_offset;
  
  /* get the first token */
  current_path = strtok(pathname, separator);
  
  struct t2fs_record *record = malloc(sizeof(struct t2fs_record));
  /* walk through other tokens */
  while( current_path != NULL ) 
  {
    while(true){
      //read first register, which is the first file in the list of files of the directory
      int first_pointer = inode->dataPtr[0];
      unsigned char *data_buffer = malloc(SECTOR_SIZE*block_size);
      sector_offset = (int) (offset/SECTOR_SIZE);

      read_sector(first_data_block + (first_pointer*block_size) + sector_offset, data_buffer);
      memcpy(record, data_buffer + offset - (sector_offset * SECTOR_SIZE), sizeof(struct t2fs_record));
      if(record->TypeVal != 1 && record->TypeVal != 2){//file doesnt exist, caller must check for validaty
        return record;
      }
      if(strcmp(record->name, current_path) == 0){//found the record
        int block = (int)((record->inodeNumber * inode_size)/SECTOR_SIZE);
	read_sector(first_inode_block + block, buffer);
	memcpy(inode, buffer + (record->inodeNumber * inode_size), sizeof(struct t2fs_inode));
        offset = 0;
        break;
      } 
      else{
        offset += record_size;
      }
    }
    current_path = strtok(NULL, separator);
  }
  return record;
}

/*-----------------------------------------------------------------------------
Função: Usada para identificar os desenvolvedores do T2FS.
	Essa função copia um string de identificação para o ponteiro indicado por "name".
	Essa cópia não pode exceder o tamanho do buffer, informado pelo parâmetro "size".
	O string deve ser formado apenas por caracteres ASCII (Valores entre 0x20 e 0x7A) e terminado por \0.
	O string deve conter o nome e número do cartão dos participantes do grupo.

Entra:	name -> buffer onde colocar o string de identificação.
	size -> tamanho do buffer "name" (número máximo de bytes a serem copiados).

Saída:	Se a operação foi realizada com sucesso, a função retorna "0" (zero).
	Em caso de erro, será retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int identify2 (char *name, int size){
  char *ident = "Adriano Benin (173464) - Lucas Valandro () - Gabriel Zilmer ()";
  memcpy(name, ident, size);
  return 0;
}


/*-----------------------------------------------------------------------------
Função: Criar um novo arquivo.
	O nome desse novo arquivo é aquele informado pelo parâmetro "filename".
	O contador de posição do arquivo (current pointer) deve ser colocado na posição zero.
	Caso já exista um arquivo ou diretório com o mesmo nome, a função deverá retornar um erro de criação.
	A função deve retornar o identificador (handle) do arquivo.
	Esse handle será usado em chamadas posteriores do sistema de arquivo para fins de manipulação do arquivo criado.

Entra:	filename -> nome do arquivo a ser criado.

Saída:	Se a operação foi realizada com sucesso, a função retorna o handle do arquivo (número positivo).
	Em caso de erro, deve ser retornado um valor negativo.
-----------------------------------------------------------------------------*/
FILE2 create2 (char *filename){
  init();

  //temp string since strtok modifies the input
  char *root_path = malloc(sizeof(char) * 256 );
  strcpy(root_path, filename);
  int handle = get_handle();
  if(handle == -1){//all occupied
    return ERROR;
  }

  struct t2fs_record *record = malloc(sizeof(struct t2fs_record));
  //check if file already exists
  record = get_record_from_path(filename);
  if(record->TypeVal == 1 || record->TypeVal == 2){
    return ERROR;
  }

  int offset = 0;
  int sector_offset;
  bool found = false;
  bool in_root = in_root_path(root_path);

  if(in_root){
    //read first inode
    struct t2fs_inode *inode = malloc(sizeof(struct t2fs_inode));
    unsigned char *buffer = malloc(SECTOR_SIZE);
    read_sector(first_inode_block, buffer);
    memcpy(inode, buffer, sizeof(struct t2fs_inode));

    int first_pointer = inode->dataPtr[0];

    while(!found){
      unsigned char *data_buffer = malloc(SECTOR_SIZE);
      struct t2fs_record *record = malloc(sizeof(struct t2fs_record));
      sector_offset = (int) (offset/SECTOR_SIZE);
      read_sector(first_data_block + (first_pointer*block_size) + sector_offset, data_buffer);
      memcpy(record, data_buffer + offset - (sector_offset * SECTOR_SIZE), sizeof(struct t2fs_record));
      if(record->TypeVal != 1 && record->TypeVal != 2){//found invalid file, append new file
        found = true;
	record->TypeVal = 1;
        record->blocksFileSize = 0;
        record->bytesFileSize = 0;
        record->bytesFileSize = 0;
        record->inodeNumber = -1;
        strcpy(record->name, get_last_name(root_path));

        OPEN_RECORD *new_record = malloc(sizeof(OPEN_RECORD));
        new_record->record = *record;
        new_record->occupied = true;
        new_record->offset = 0;
        open_records[handle] = *new_record;
        memcpy(data_buffer + offset - (sector_offset * SECTOR_SIZE), record, record_size);
        write_sector(first_data_block + (first_pointer*block_size) + sector_offset, data_buffer);
        return handle;
      }
      offset += record_size;
    } 

    return ERROR;
  }else {
      struct t2fs_record *record = malloc(sizeof(struct t2fs_record));
      record = get_last_record(root_path);

      struct t2fs_inode *inode = malloc(sizeof(struct t2fs_inode));
      unsigned char *buffer = malloc(SECTOR_SIZE);
      int sector = (int)((record->inodeNumber * inode_size)/SECTOR_SIZE);
      read_sector(first_inode_block + sector, buffer);
      memcpy(inode, buffer + (record->inodeNumber * inode_size), sizeof(struct t2fs_inode));

      int first_pointer = inode->dataPtr[0];

    //@TODO refactor
      while(!found){
        unsigned char *data_buffer = malloc(SECTOR_SIZE);
        struct t2fs_record *record = malloc(sizeof(struct t2fs_record));
        sector_offset = (int) (offset/SECTOR_SIZE);
        read_sector(first_data_block + (first_pointer*block_size) + sector_offset, data_buffer);
        memcpy(record, data_buffer + offset - (sector_offset * SECTOR_SIZE), sizeof(struct t2fs_record));
        if(record->TypeVal != 1 && record->TypeVal != 2){//found invalid file, append new file
          found = true;
	  record->TypeVal = 1;
          record->blocksFileSize = 0;
          record->bytesFileSize = 0;
          record->bytesFileSize = 0;
          record->inodeNumber = -1;
          strcpy(record->name, get_last_name(root_path));

          OPEN_RECORD *new_record = malloc(sizeof(OPEN_RECORD));
          new_record->record = *record;
          new_record->occupied = true;
          new_record->offset = 0;
          open_records[handle] = *new_record;
          memcpy(data_buffer + offset - (sector_offset * SECTOR_SIZE), record, record_size);
          write_sector(first_data_block + (first_pointer*block_size) + sector_offset, data_buffer);
          return handle;
        }
        offset += record_size;
      }
  }

  return handle;
}


/*-----------------------------------------------------------------------------
Função:	Apagar um arquivo do disco.
	O nome do arquivo a ser apagado é aquele informado pelo parâmetro "filename".

Entra:	filename -> nome do arquivo a ser apagado.

Saída:	Se a operação foi realizada com sucesso, a função retorna "0" (zero).
	Em caso de erro, será retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int delete2 (char *filename){
  init();
  return ERROR;
}


/*-----------------------------------------------------------------------------
Função:	Abre um arquivo existente no disco.
	O nome desse novo arquivo é aquele informado pelo parâmetro "filename".
	Ao abrir um arquivo, o contador de posição do arquivo (current pointer) deve ser colocado na posição zero.
	A função deve retornar o identificador (handle) do arquivo.
	Esse handle será usado em chamadas posteriores do sistema de arquivo para fins de manipulação do arquivo criado.
	Todos os arquivos abertos por esta chamada são abertos em leitura e em escrita.
	O ponto em que a leitura, ou escrita, será realizada é fornecido pelo valor current_pointer (ver função seek2).

Entra:	filename -> nome do arquivo a ser apagado.

Saída:	Se a operação foi realizada com sucesso, a função retorna o handle do arquivo (número positivo)
	Em caso de erro, deve ser retornado um valor negativo
-----------------------------------------------------------------------------*/
FILE2 open2 (char *filename){
  init();

  int handle = get_handle();
  if(handle == -1){//all occupied
    return ERROR;
  }

  struct t2fs_record *record = malloc(sizeof(struct t2fs_record));

  OPEN_RECORD *new_record = malloc(sizeof(OPEN_RECORD));
  new_record->record = *get_record_from_path(filename);
  if(new_record->record.TypeVal != 1 && new_record->record.TypeVal != 2){//file doesnt exist
    return ERROR;
  }
  new_record->occupied = true;
  new_record->offset = 0;
  open_records[handle] = *new_record;

  return handle;
}


/*-----------------------------------------------------------------------------
Função:	Fecha o arquivo identificado pelo parâmetro "handle".

Entra:	handle -> identificador do arquivo a ser fechado

Saída:	Se a operação foi realizada com sucesso, a função retorna "0" (zero).
	Em caso de erro, será retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int close2 (FILE2 handle){
  init();
  return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Realiza a leitura de "size" bytes do arquivo identificado por "handle".
	Os bytes lidos são colocados na área apontada por "buffer".
	Após a leitura, o contador de posição (current pointer) deve ser ajustado para o byte seguinte ao último lido.

Entra:	handle -> identificador do arquivo a ser lido
	buffer -> buffer onde colocar os bytes lidos do arquivo
	size -> número de bytes a serem lidos

Saída:	Se a operação foi realizada com sucesso, a função retorna o número de bytes lidos.
	Se o valor retornado for menor do que "size", então o contador de posição atingiu o final do arquivo.
	Em caso de erro, será retornado um valor negativo.
-----------------------------------------------------------------------------*/
int read2 (FILE2 handle, char *buffer, int size){
  init();

  unsigned char *inode_buffer = malloc(SECTOR_SIZE);
  unsigned char *data_buffer = malloc(SECTOR_SIZE*block_size);
  struct t2fs_inode *data_inode = malloc(sizeof(struct t2fs_inode));
  OPEN_RECORD *open_record = &open_records[handle];
  struct t2fs_record record = open_record->record;

  int block = (int)((record.inodeNumber * inode_size)/SECTOR_SIZE);
  read_sector(first_inode_block + block, inode_buffer);
  memcpy(data_inode, inode_buffer + (record.inodeNumber * inode_size), sizeof(struct t2fs_inode));

  //@TODO case when file is bigger than one sector or block
  int file_data_pointer = data_inode->dataPtr[0];
  read_sector(first_data_block + (file_data_pointer*block_size), data_buffer);

  memcpy(buffer, data_buffer + open_record->offset, size);
  open_record->offset += size;
  //@TODO return only the amount of bytes read
  return size;
}


/*-----------------------------------------------------------------------------
Função:	Realiza a escrita de "size" bytes no arquivo identificado por "handle".
	Os bytes a serem escritos estão na área apontada por "buffer".
	Após a escrita, o contador de posição (current pointer) deve ser ajustado para o byte seguinte ao último escrito.

Entra:	handle -> identificador do arquivo a ser escrito
	buffer -> buffer de onde pegar os bytes a serem escritos no arquivo
	size -> número de bytes a serem escritos

Saída:	Se a operação foi realizada com sucesso, a função retorna o número de bytes efetivamente escritos.
	Em caso de erro, será retornado um valor negativo.
-----------------------------------------------------------------------------*/
int write2 (FILE2 handle, char *buffer, int size){
  return ERROR;
}


/*-----------------------------------------------------------------------------


Função:	Função usada para truncar um arquivo.
	Remove do arquivo todos os bytes a partir da posição atual do contador de posição (current pointer)
	Todos os bytes desde a posição indicada pelo current pointer até o final do arquivo são removidos do arquivo.

Entra:	handle -> identificador do arquivo a ser truncado

Saída:	Se a operação foi realizada com sucesso, a função retorna "0" (zero).
	Em caso de erro, será retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int truncate2 (FILE2 handle){
  return ERROR;
}


/*-----------------------------------------------------------------------------
Função:	Reposiciona o contador de posições (current pointer) do arquivo identificado por "handle".
	A nova posição é determinada pelo parâmetro "offset".
	O parâmetro "offset" corresponde ao deslocamento, em bytes, contados a partir do início do arquivo.
	Se o valor de "offset" for "-1", o current_pointer deverá ser posicionado no byte seguinte ao final do arquivo,
		Isso é útil para permitir que novos dados sejam adicionados no final de um arquivo já existente.

Entra:	handle -> identificador do arquivo a ser escrito
	offset -> deslocamento, em bytes, onde posicionar o "current pointer".

Saída:	Se a operação foi realizada com sucesso, a função retorna "0" (zero).
	Em caso de erro, será retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int seek2 (FILE2 handle, DWORD offset){
  return ERROR;
}


/*-----------------------------------------------------------------------------
Função:	Criar um novo diretório.
	O caminho desse novo diretório é aquele informado pelo parâmetro "pathname".
		O caminho pode ser ser absoluto ou relativo.
	A criação de um novo subdiretório deve ser acompanhada pela criação, automática, das entradas "." e ".."
	A entrada "." corresponde ao descritor do subdiretório recém criado
	A entrada ".." corresponde à entrada de seu diretório pai.
	São considerados erros de criação quaisquer situações em que o diretório não possa ser criado.
		Isso inclui a existência de um arquivo ou diretório com o mesmo "pathname".

Entra:	pathname -> caminho do diretório a ser criado

Saída:	Se a operação foi realizada com sucesso, a função retorna "0" (zero).
	Em caso de erro, será retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int mkdir2 (char *pathname){
  return ERROR;
}


/*-----------------------------------------------------------------------------
Função:	Apagar um subdiretório do disco.
	O caminho do diretório a ser apagado é aquele informado pelo parâmetro "pathname".
	São considerados erros quaisquer situações que impeçam a operação.
		Isso inclui:
			(a) o diretório a ser removido não está vazio;
			(b) "pathname" não existente;
			(c) algum dos componentes do "pathname" não existe (caminho inválido);
			(d) o "pathname" indicado não é um arquivo;
			(e) o "pathname" indica os diretórios "." ou "..".

Entra:	pathname -> caminho do diretório a ser criado

Saída:	Se a operação foi realizada com sucesso, a função retorna "0" (zero).
	Em caso de erro, será retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int rmdir2 (char *pathname){
  return ERROR;
}


/*-----------------------------------------------------------------------------
Função:	Abre um diretório existente no disco.
	O caminho desse diretório é aquele informado pelo parâmetro "pathname".
	Se a operação foi realizada com sucesso, a função:
		(a) deve retornar o identificador (handle) do diretório
		(b) deve posicionar o ponteiro de entradas (current entry) na primeira posição válida do diretório "pathname".
	O handle retornado será usado em chamadas posteriores do sistema de arquivo para fins de manipulação do diretório.

Entra:	pathname -> caminho do diretório a ser aberto

Saída:	Se a operação foi realizada com sucesso, a função retorna o identificador do diretório (handle).
	Em caso de erro, será retornado um valor negativo.
-----------------------------------------------------------------------------*/
DIR2 opendir2 (char *pathname){
  init();

  int handle = get_handle();
  if(handle == -1){//all occupied
    return ERROR;
  }

  if(strcmp(pathname, "/") == 0){
    struct t2fs_record *root_record = malloc(sizeof(struct t2fs_record));
    root_record->TypeVal= 2;
    strcpy(root_record->name, "/");
    root_record->inodeNumber = 0;

    OPEN_RECORD *new_record = malloc(sizeof(OPEN_RECORD));
    new_record->record = *root_record;
    new_record->occupied = true;
    new_record->offset = 0;
    open_records[handle] = *new_record;
    return handle;
  }

  struct t2fs_record *record = malloc(sizeof(struct t2fs_record));

  OPEN_RECORD *new_record = malloc(sizeof(OPEN_RECORD));
  new_record->record = *get_record_from_path(pathname);
  if(new_record->record.TypeVal != 1 && new_record->record.TypeVal != 2){//file doesnt exist
    return ERROR;
  }
  new_record->occupied = true;
  new_record->offset = 0;
  open_records[handle] = *new_record;

  return handle;
}


/*-----------------------------------------------------------------------------
Função:	Realiza a leitura das entradas do diretório identificado por "handle".
	A cada chamada da função é lida a entrada seguinte do diretório representado pelo identificador "handle".
	Algumas das informações dessas entradas devem ser colocadas no parâmetro "dentry".
	Após realizada a leitura de uma entrada, o ponteiro de entradas (current entry) deve ser ajustado para a próxima entrada válida, seguinte à última lida.
	São considerados erros:
		(a) qualquer situação que impeça a realização da operação
		(b) término das entradas válidas do diretório identificado por "handle".

Entra:	handle -> identificador do diretório cujas entradas deseja-se ler.
	dentry -> estrutura de dados onde a função coloca as informações da entrada lida.

Saída:	Se a operação foi realizada com sucesso, a função retorna "0" (zero).
	Em caso de erro, será retornado um valor diferente de zero ( e "dentry" não será válido)
-----------------------------------------------------------------------------*/
int readdir2 (DIR2 handle, DIRENT2 *dentry){
  init();
  OPEN_RECORD *dir = &open_records[handle];

  //read first inode
  struct t2fs_inode *inode = malloc(sizeof(struct t2fs_inode));
  unsigned char *buffer = malloc(SECTOR_SIZE);
  int block = (int)((dir->record.inodeNumber * inode_size)/SECTOR_SIZE);
  read_sector(first_inode_block + block, buffer);
  memcpy(inode, buffer + (dir->record.inodeNumber * inode_size), sizeof(struct t2fs_inode));

  //read first register, which is the first file in the list of files of the directory
  int sector_offset = (int) (dir->offset/SECTOR_SIZE);
  int first_pointer = inode->dataPtr[0];
  unsigned char *data_buffer = malloc(SECTOR_SIZE*block_size);
  struct t2fs_record *record = malloc(sizeof(struct t2fs_record));

  read_sector(first_data_block + (first_pointer*block_size) + sector_offset, data_buffer);
  memcpy(record, data_buffer + dir->offset - (sector_offset * SECTOR_SIZE), sizeof(struct t2fs_record));
  if(record->TypeVal != 1 && record->TypeVal != 2){return ERROR;}
  strcpy(dentry->name, record->name);
  dentry->fileType = record->TypeVal;
  dentry->fileSize = record->bytesFileSize;

  dir->offset += record_size;
  return 0;
}


/*-----------------------------------------------------------------------------
Função:	Fecha o diretório identificado pelo parâmetro "handle".

Entra:	handle -> identificador do diretório que se deseja fechar (encerrar a operação).

Saída:	Se a operação foi realizada com sucesso, a função retorna "0" (zero).
	Em caso de erro, será retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int closedir2 (DIR2 handle){
  init();
  return ERROR;
}

