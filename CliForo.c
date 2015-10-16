#include <stdio.h> 
#include <unistd.h>
#include <string.h>
#include <sys/file.h>
#include <errno.h>	
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <signal.h>

#define MAX_COMAND 300
#define MAX_NAME 50

//NOMBRES DE RECURSOS COMPARTIDOS
#define SHM_PATH         "/ServiForoSharedMemory"
#define SEM_CLI_NAME "ServiForoCliSem"
#define SEM_CMD_NAME "ServiForoCmd"
#define SEM_CLIPARAM_NAME "ServiForoCliParam"
#define SEM_SERVIMSG_NAME "ServiForoServiMsg"

//ESTRUCTURA DE DATOS COMPARTIDA
struct shared_data {
    int cmd;
    /*Comandos:
    1 - Registrar Cliente
    2 - Salir cliente
    3 - List
    4 - Write
    5 - Read
    */ 
    char cmdParam[MAX_COMAND];
    char serviMsg[MAX_COMAND];
};

//FUNCION QUE LIBERA LOS RECURSOS
void unlinks()
{
	shm_unlink(SEM_CLI_NAME);
}

int main(int argc, char * argv[])
{
	//COMPROBAR QUE EL SERVIDOR ESTA LEVANTADO
	sem_t * sem_cli_id = sem_open(SEM_CLI_NAME, 0, 600, 1);
    sem_t * sem_cmd_id = sem_open(SEM_CMD_NAME, 0, 600, 1);
    sem_t * sem_cliParam_id = sem_open(SEM_CLIPARAM_NAME, 0, 600, 1);
    sem_t * sem_ServiMsg_id = sem_open(SEM_SERVIMSG_NAME, 0, 600, 1);
	if ((sem_cli_id == SEM_FAILED)  || (sem_cmd_id == SEM_FAILED) || (sem_cliParam_id == SEM_FAILED) || (sem_ServiMsg_id == SEM_FAILED))
	{
		printf("Error, no exite una instancia de ServiForo en ejecución\n\n");
  		unlinks();
  		return -1;
	}
	
	//File desciptor
	int shmfd;
	//Tama;o de la estructura
    int shared_seg_size = (1 * sizeof(struct shared_data)); 
    //Estructura compartida
    struct shared_data *shared_msg;
     //Creando el objeto de memoria compartido /* creating the shared memory object    --  shm_open()  */
    shmfd = shm_open("/ServiForoSharedMemory", O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
    if (shmfd < 0)
    {
        perror("Error en shm_open()");
        unlinks();
  		return -1;
    }
    //Ajustando el tama;o del mapeo al tama;o de la estructura
    ftruncate(shmfd, shared_seg_size);
    //Solicitando el segmento compartido    
	shared_msg = (struct shared_data *)mmap(NULL, shared_seg_size, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if (shared_msg == NULL)
    {
       	printf("Error, no exite una instancia de ServiForo en ejecución\n\n");
        unlinks();
        return -1;
    }

	//NOMBRE DE USUARIO
	char name[MAX_NAME];
	if (argc == 1)
	{
		printf("Debe ingresar un nombre de usuario\n\n");
		unlinks();
		return -1;
	}
	else 
	{		
		strcpy(name, argv[1]);
		//obtener semaforo de cliente
		sem_wait(sem_cli_id);
		
		//poner nombre como parametro
		sem_wait(sem_cliParam_id);
		strcpy(shared_msg->cmdParam, name);
		sem_post(sem_cliParam_id);

		//Llamar comando Registrar Cliente
		sem_wait(sem_cmd_id);
		shared_msg->cmd=1;
		sem_post(sem_cmd_id);

		//Esperar por respuesta
		while(1)
		{
			sem_wait(sem_ServiMsg_id);
			if (strncmp(shared_msg->serviMsg,"nnn", 3))
			{
				if (!strncmp(shared_msg->serviMsg,"ok", 2))
				{
					strcpy(shared_msg->serviMsg, "nnn");
					break;	
				}
				else 
				{
					printf("%s", shared_msg->serviMsg);
					strcpy(shared_msg->serviMsg, "nnn");
					unlinks();
					return -1;
				}
			}
			sem_post(sem_ServiMsg_id);
		}

		sem_post(sem_cli_id);
	}
	

	//ARRANCAR EL CLIENTE
	printf("Bienvenido a CliForo!\n");
	printf("Su nombre es %s\n\n", name);

	//ESPERAR POR UN COMANDO
	char comand[MAX_COMAND] = "";
	while (1)
	{
		//LEER COMANDO
		printf(">> ");
		fgets(comand, MAX_COMAND , stdin);

		//COMANDO EXIT
		if (!strncmp(comand,"exit", 4))
		{
			//SALIR
			printf("Hasta pronto!\n");
			unlinks();
			return 1;
		} 
		//COMANDO INVALIDO
		else 
		{
			printf("Comando inválido!\n");
		}
	}

	//SALIR
	unlinks();
	return 1;
}