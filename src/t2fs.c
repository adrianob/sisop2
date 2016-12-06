#include "../include/t2fs.h"
#include "../include/apidisk.h"
#include "../include/bitmap2.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

static int initialized = false;
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

//ex: /a/b/c will return /a/b
char * get_last_path(char *pathname){
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
  return last_path;
}

//ex: /a/b/c will return the record for /a/b
struct t2fs_record * get_last_record(char *pathname){
  char *name = malloc(sizeof(char) * 256 );
  strcpy(name, pathname);
  OPEN_RECORD *temp = get_record_from_path(get_last_path(name));
  return &temp->record;
}

//ex: /a/b/c will return c
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

//get the first inode for the given path
//root path will return inode zero, other paths will return the inode in inodeNumber
struct t2fs_inode * get_first_inode(root_path){
  bool in_root = in_root_path(root_path);

  struct t2fs_inode *inode;
  unsigned char *buffer;
  buffer = malloc(SECTOR_SIZE);
  if(in_root){
    //read first inode
    inode = malloc(sizeof(struct t2fs_inode));
    read_sector(first_inode_block, buffer);
    memcpy(inode, buffer, sizeof(struct t2fs_inode));
  }else{
    struct t2fs_record *record = malloc(sizeof(struct t2fs_record));
    record = get_last_record(root_path);

    inode = malloc(sizeof(struct t2fs_inode));
    int sector = (int)((record->inodeNumber * inode_size)/SECTOR_SIZE);
    read_sector(first_inode_block + sector, buffer);
    memcpy(inode, buffer + (record->inodeNumber * inode_size), sizeof(struct t2fs_inode));
  }

  return inode;
}

//check if the file is in the root directory
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

//return an open_record from given path
OPEN_RECORD * get_record_from_path(char *pathname){
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
  int first_pointer;
  /* walk through other tokens */
  while( current_path != NULL )
  {
    while(true){
      //read first register, which is the first file in the list of files of the directory
      first_pointer = inode->dataPtr[0];
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
        if(record->TypeVal == 2){
          offset = 0;
        }
        break;
      }
      else{
        offset += record_size;
      }
    }
    current_path = strtok(NULL, separator);
  }

  OPEN_RECORD *new_record = malloc(sizeof(OPEN_RECORD));
  new_record->record = *record;
  new_record->occupied = true;
  new_record->initial_sector = first_data_block + (first_pointer*block_size) + sector_offset;
  new_record->sector_offset = offset - (sector_offset * SECTOR_SIZE);
  new_record->offset = 0;
  return new_record;
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
  char *ident = "Adriano Benin {00173464} - Lucas Valandro {00243675} - Gabriel Zillmer {00243683}";
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
  OPEN_RECORD *temp = get_record_from_path(filename);
  record = &temp->record;
  if(record->TypeVal == 1 || record->TypeVal == 2){
    return ERROR;
  }

  int offset = 0;
  int sector_offset;
  bool found = false;

  struct t2fs_inode *inode = get_first_inode(root_path);
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
      record->inodeNumber = searchBitmap2(BITMAP_INODE, 0);
      setBitmap2(BITMAP_INODE, record->inodeNumber, 1);
      strcpy(record->name, get_last_name(root_path));

      OPEN_RECORD *new_record = malloc(sizeof(OPEN_RECORD));
      new_record->record = *record;
      new_record->occupied = true;
      new_record->initial_sector = first_data_block + (first_pointer*block_size) + sector_offset;
      new_record->sector_offset = offset - (sector_offset * SECTOR_SIZE);
      new_record->offset = 0;
      open_records[handle] = *new_record;
      memcpy(data_buffer + offset - (sector_offset * SECTOR_SIZE), record, record_size);
      write_sector(first_data_block + (first_pointer*block_size) + sector_offset, data_buffer);
      return handle;
    }
    offset += record_size;
  }

  return ERROR;
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

  //temp string since strtok modifies the input
  char *root_path = malloc(sizeof(char) * 256 );
  strcpy(root_path, filename);

  struct t2fs_record *record = malloc(sizeof(struct t2fs_record));
  //check if file exists
  OPEN_RECORD *temp = get_record_from_path(filename);
  record = &temp->record;
  if(record->TypeVal != 1 && record->TypeVal != 2){
    return ERROR;
  }
  record->TypeVal = 0;//mark file as invalid
  //set bitmaps to zero
  setBitmap2(BITMAP_INODE, record->inodeNumber, 0);
  struct t2fs_inode *inode = get_first_inode(root_path);
  int first_pointer = inode->dataPtr[0];
  int second_pointer = inode->dataPtr[1];
  setBitmap2(BITMAP_DADOS, first_pointer, 0);
  if(second_pointer != INVALID_PTR){
    setBitmap2(BITMAP_DADOS, second_pointer, 0);
  }

  //get last record on dir
  DIR2 d;
  d = opendir2(get_last_path(root_path));
  DIRENT2 *dentry = malloc(sizeof(DIRENT2));
  bool should_append = true;
  while ( readdir2(d, dentry) == 0 );
  char *last_path = malloc(sizeof(char) * 256 );
  if(strcmp(get_last_path(root_path), "") == 0){
    should_append = false;
  }
  strcat(last_path,get_last_path(root_path));
  if(should_append){
    strcat(last_path,"/");
  }

  strcat(last_path,dentry->name);

  closedir2(d);

  struct t2fs_record *last_record = malloc(sizeof(struct t2fs_record));
  OPEN_RECORD *last_open = get_record_from_path(last_path);
  last_record = &last_open->record;
  //file is not the last in dir, switch places
  if(strcmp(filename, last_path) != 0){
    //write last one in place of deleted file
    unsigned char *data_buffer = malloc(SECTOR_SIZE);
    read_sector(temp->initial_sector, data_buffer);
    memcpy(data_buffer + temp->sector_offset, last_record, record_size);
    write_sector(temp->initial_sector, data_buffer);
    //clear last file
    last_record->TypeVal = 0;
    unsigned char *buffer = malloc(SECTOR_SIZE);
    read_sector(last_open->initial_sector, buffer);
    memcpy(buffer + last_open->sector_offset, last_record, record_size);
    write_sector(last_open->initial_sector, buffer);
  } else {
    //write updated record to disk
    unsigned char *data_buffer = malloc(SECTOR_SIZE);
    read_sector(temp->initial_sector, data_buffer);
    memcpy(data_buffer + temp->sector_offset, record, record_size);
    write_sector(temp->initial_sector, data_buffer);
  }

  return SUCCESS;
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
  new_record = get_record_from_path(filename);
  if(new_record->record.TypeVal != 1 && new_record->record.TypeVal != 2){//file doesnt exist
    return ERROR;
  }
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
  OPEN_RECORD *file = &open_records[handle];
  OPEN_RECORD *open_record = &open_records[handle];
  struct t2fs_record record = open_record->record;
  if(handle < 20){ //valid handle
    if(file->record.TypeVal == 1){//it's a file
        file->occupied = false;
        write2(handle,file,sizeof(OPEN_RECORD));
        return SUCCESS;
    }
   else
    return ERROR;
  }
  else
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
  struct t2fs_inode *data_inode = malloc(sizeof(struct t2fs_inode));
  OPEN_RECORD *open_record = &open_records[handle];
  struct t2fs_record record = open_record->record;

  int sector = (int)((record.inodeNumber * inode_size)/SECTOR_SIZE);
  read_sector(first_inode_block + sector, inode_buffer);
  memcpy(data_inode, inode_buffer + (record.inodeNumber * inode_size), sizeof(struct t2fs_inode));

  int first_data_pointer = data_inode->dataPtr[0];
  int second_data_pointer = data_inode->dataPtr[1];
  int single_indirection_ptr = data_inode->singleIndPtr;
  int double_indirection_ptr = data_inode->doubleIndPtr;
  int block_size_bytes = SECTOR_SIZE*block_size;
  int max_file_size = block_size_bytes * ((block_size_bytes/4)*(block_size_bytes/4) + (block_size_bytes/4) + 2);
  unsigned char *data_buffer = malloc(max_file_size);

  int i;
  //read the whole first block into the buffer
  for(i=0;i<block_size;i++){
    read_sector(first_data_block + (first_data_pointer*block_size) + i, data_buffer + SECTOR_SIZE * i);
  }

  if(second_data_pointer != INVALID_PTR){
    //read the whole second block into the buffer
    for(i=0;i<block_size;i++){
     read_sector(first_data_block + (second_data_pointer*block_size) + i, data_buffer + SECTOR_SIZE * i + (SECTOR_SIZE*block_size));
    }
  }

  if(size > (record.bytesFileSize - open_record->offset)){
    size = record.bytesFileSize - open_record->offset;
  }

  if((record.bytesFileSize - open_record->offset) == 0){
    return ERROR;
  }
  memcpy(buffer, data_buffer + open_record->offset, size);
  open_record->offset += size;
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
 if(open_records[handle].occupied == true){
  init();

  unsigned char *inode_buffer = malloc(SECTOR_SIZE);
  struct t2fs_inode *data_inode = malloc(sizeof(struct t2fs_inode));
  OPEN_RECORD *file = &open_records[handle];
  struct t2fs_record *record = &file->record;

  int sector = (int)((record->inodeNumber * inode_size)/SECTOR_SIZE);
  read_sector(first_inode_block + sector, inode_buffer);
  memcpy(data_inode, inode_buffer + (record->inodeNumber * inode_size), sizeof(struct t2fs_inode));

  data_inode->dataPtr[0] = searchBitmap2(BITMAP_DADOS, 0);
  int first_data_pointer = data_inode->dataPtr[0];
  int second_data_pointer = data_inode->dataPtr[1];
  int single_indirection_ptr = data_inode->singleIndPtr;
  int double_indirection_ptr = data_inode->doubleIndPtr;
  int block_size_bytes = SECTOR_SIZE*block_size;
  int max_file_size = block_size_bytes * ((block_size_bytes/4)*(block_size_bytes/4) + (block_size_bytes/4) + 2);
  unsigned char *data_buffer = malloc(max_file_size);

  int i;
  //write the buffer into whole first block
  for(i=0;i<block_size;i++){
    read_sector(file->initial_sector + i, data_buffer + SECTOR_SIZE * i);
    //memcpy(record, buffer, size);
    memcpy(data_buffer + file->sector_offset, record, record_size);
    write_sector(file->initial_sector + i, data_buffer + SECTOR_SIZE * i);
  }
 //write the buffer into whole second block
  if(second_data_pointer != INVALID_PTR){
    for(i=0;i<block_size;i++){
     write_sector(first_data_block + (second_data_pointer*block_size) + i, buffer + SECTOR_SIZE * i + (SECTOR_SIZE*block_size));
    }
  }
  }
  else
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
  init();
  OPEN_RECORD *file = &open_records[handle];

  struct t2fs_record *record = malloc(sizeof(struct t2fs_record));
  record = &file->record;
  record->bytesFileSize = file->offset;
  record->blocksFileSize = (int)(record->bytesFileSize/(SECTOR_SIZE*block_size));

  unsigned char *buffer = malloc(SECTOR_SIZE);
  read_sector(file->initial_sector, buffer);
  memcpy(buffer + file->sector_offset, record, record_size);
  write_sector(file->initial_sector, buffer);
  return SUCCESS;
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
  init();
  OPEN_RECORD *file = &open_records[handle];

  if(offset == -1){
    struct t2fs_record *record = malloc(sizeof(struct t2fs_record));
    record = &file->record;
    file->offset = record->bytesFileSize;
  }
  else{
    file->offset = offset;
  }
  return SUCCESS;
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
  init();

  //temp string since strtok modifies the input
  char *root_path = malloc(sizeof(char) * 256 );
  strcpy(root_path, pathname);

  struct t2fs_record *record = malloc(sizeof(struct t2fs_record));
  //check if file already exists
  OPEN_RECORD *temp = get_record_from_path(pathname);
  record = &temp->record;
  if(record->TypeVal == 1 || record->TypeVal == 2){
    return ERROR;
  }

  int offset = 0;
  int sector_offset;
  bool found = false;

  struct t2fs_inode *inode = get_first_inode(root_path);
  int first_pointer = inode->dataPtr[0];

  while(!found){
    unsigned char *data_buffer = malloc(SECTOR_SIZE);
    struct t2fs_record *record = malloc(sizeof(struct t2fs_record));
    sector_offset = (int) (offset/SECTOR_SIZE);
    read_sector(first_data_block + (first_pointer*block_size) + sector_offset, data_buffer);
    memcpy(record, data_buffer + offset - (sector_offset * SECTOR_SIZE), sizeof(struct t2fs_record));
    if(record->TypeVal != 1 && record->TypeVal != 2){//found invalid file, append new file
      found = true;
      record->TypeVal = 2;
      record->blocksFileSize = 0;
      record->bytesFileSize = 0;
      record->inodeNumber = searchBitmap2(BITMAP_INODE, 0);
      setBitmap2(BITMAP_INODE, record->inodeNumber, 1);
      strcpy(record->name, get_last_name(root_path));

      memcpy(data_buffer + offset - (sector_offset * SECTOR_SIZE), record, record_size);
      write_sector(first_data_block + (first_pointer*block_size) + sector_offset, data_buffer);
      return SUCCESS;
    }
    offset += record_size;
  }

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
  init();

  //temp string since strtok modifies the input
  char *root_path = malloc(sizeof(char) * 256 );
  strcpy(root_path, pathname);

  struct t2fs_record *record = malloc(sizeof(struct t2fs_record));
  //check if file exists
  OPEN_RECORD *temp = get_record_from_path(pathname);
  record = &temp->record;
  if(record->TypeVal != 1 && record->TypeVal != 2){
    return ERROR;
  }
  record->TypeVal = 0;//mark file as invalid
  //set bitmaps to zero
  setBitmap2(BITMAP_INODE, record->inodeNumber, 0);
  struct t2fs_inode *inode = get_first_inode(root_path);
  int first_pointer = inode->dataPtr[0];
  int second_pointer = inode->dataPtr[1];
  setBitmap2(BITMAP_DADOS, first_pointer, 0);
  if(second_pointer != INVALID_PTR){
    setBitmap2(BITMAP_DADOS, second_pointer, 0);
  }

  //check if folder is empty
  DIR2 temp_dir;
  temp_dir = opendir2(root_path);
  DIRENT2 *dentry_temp = malloc(sizeof(DIRENT2));
  if ( readdir2(temp_dir, dentry_temp) == 0 ){
    return ERROR;
  }

  //get last record on dir
  DIR2 d;
  d = opendir2(get_last_path(root_path));
  DIRENT2 *dentry = malloc(sizeof(DIRENT2));
  bool should_append = true;
  while ( readdir2(d, dentry) == 0 );
  char *last_path = malloc(sizeof(char) * 256 );
  if(strcmp(get_last_path(root_path), "") == 0){
    should_append = false;
  }
  strcat(last_path,get_last_path(root_path));
  if(should_append){
    strcat(last_path,"/");
  }

  strcat(last_path,dentry->name);

  closedir2(d);

  struct t2fs_record *last_record = malloc(sizeof(struct t2fs_record));
  OPEN_RECORD *last_open = get_record_from_path(last_path);
  last_record = &last_open->record;
  //file is not the last in dir, switch places
  if(strcmp(pathname, last_path) != 0){
    //write last one in place of deleted file
    unsigned char *data_buffer = malloc(SECTOR_SIZE);
    read_sector(temp->initial_sector, data_buffer);
    memcpy(data_buffer + temp->sector_offset, last_record, record_size);
    write_sector(temp->initial_sector, data_buffer);
    //clear last file
    last_record->TypeVal = 0;
    unsigned char *buffer = malloc(SECTOR_SIZE);
    read_sector(last_open->initial_sector, buffer);
    memcpy(buffer + last_open->sector_offset, last_record, record_size);
    write_sector(last_open->initial_sector, buffer);
  } else {
    //write updated record to disk
    unsigned char *data_buffer = malloc(SECTOR_SIZE);
    read_sector(temp->initial_sector, data_buffer);
    memcpy(data_buffer + temp->sector_offset, record, record_size);
    write_sector(temp->initial_sector, data_buffer);
  }

  return SUCCESS;
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
  new_record = get_record_from_path(pathname);
  if(new_record->record.TypeVal != 1 && new_record->record.TypeVal != 2){//file doesnt exist
    return ERROR;
  }
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
  OPEN_RECORD *dir = &open_records[handle];
  dir->occupied = false;
  return SUCCESS;
}
