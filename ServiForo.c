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

//CONSTANTES CONFIGURABLES
#define MAX_MENSAJES_EN_FORO 100
#define MAX_CLIENTES 20
#define MAX_LARGO_MENSAJE 200
#define MAX_COMAND 300
#define MAX_NAME 10

#define SEM_INSTANCE_NAME "ServiForoInstanceSem"
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
    2 - List
    3 - Write
    3 - Read
    */ 
    char cmdParam[MAX_COMAND];
    char serviMsg[MAX_COMAND];
};

//ESTRUCTURA DEL SERVIDOR
//Array de clientes
char clientes[MAX_CLIENTES][MAX_NAME];
int cliCount = -1;

//FUNCION QUE LIBERA LOS RECURSOS
int existUser(const char name[MAX_NAME])
{
	int res = 0;
	int i = 0;
	while (i < cliCount)
	{
		if (!strcmp(name,clientes[i]))
		{
			res = 1;
			break;
		}
	}
	return res;
}

//FUNCION QUE LIBERA LOS RECURSOS
void unlinks()
{
	shm_unlink(SEM_CLI_NAME);
	shm_unlink(SHM_PATH);
	sem_unlink(SEM_INSTANCE_NAME);
}

int main()
{
	//SI YA HAY OTRA INSTANCIA EN EJECUCION SALGO
	if (sem_open(SEM_INSTANCE_NAME, O_CREAT, 600, 1) == SEM_FAILED) 
	{
		printf("Ya existe una instancia de ServiForo en ejecución\n\n");
  		unlinks();
  		return -1;
	}
	
	//ARRANCANDO SERVIDOR
	printf("Arrancando Servidor...\n");
	
	//File desciptor
	int shmfd;
	//Tama;o de la estructura
    int shared_seg_size = (1 * sizeof(struct shared_data)); 
    //Estructura compartida
    struct shared_data *shared_msg;     
	
	//Creando la memoria compartida
	shmfd = shm_open(SHM_PATH, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
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
        perror("Error en mmap()");
        return -1;
    }
   	//Inicializar estructura
   	shared_msg->cmd = 0;
   	strcpy(shared_msg->cmdParam, "nnn");
   	strcpy(shared_msg->serviMsg, "nnn");

    //Creando semaforo para clientes
    sem_open(SEM_CLI_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1);
    sem_t * sem_cmd_id;
    sem_t * sem_cliParam_id;
    sem_t * sem_ServiMsg_id;
    sem_cmd_id = sem_open(SEM_CMD_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1);
    sem_cliParam_id = sem_open(SEM_CLIPARAM_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1);
    sem_ServiMsg_id = sem_open(SEM_SERVIMSG_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1);

	//FORK
    pid_t pid = fork();
    if (pid == 0)
    //PROCESO HIJO ATIENDE A LOS CLIENTES
    {
        while (1)
		{
			sem_wait(sem_cmd_id);
			if (shared_msg->cmd !=0)
			//hay un comando
			{

				if (shared_msg->cmd == 1)
				//Registrar Cliente
				{
					//obtener nombre
					char name[MAX_NAME];
					sem_wait(sem_cliParam_id);
					strncpy(name, shared_msg->cmdParam, MAX_NAME);
					strcpy(shared_msg->cmdParam, "nnn");
					sem_post(sem_cliParam_id);

					if (cliCount + 1 > MAX_CLIENTES)
					{
						//ya existe
						sem_wait(sem_ServiMsg_id);
						strcpy(shared_msg->serviMsg, "Ya no se permiten mas usuarios\n");
						sem_post(sem_ServiMsg_id);
					}
					else if (existUser(name))
					{
						//ya existe
						sem_wait(sem_ServiMsg_id);
						strcpy(shared_msg->serviMsg, "Este nombre de usuario ya está en uso\n");
						sem_post(sem_ServiMsg_id);
					}
					else
					{
						//Exito
						cliCount++;
						strcpy(clientes[cliCount], name);

						sem_wait(sem_ServiMsg_id);
						strcpy(shared_msg->serviMsg, "ok");
						sem_post(sem_ServiMsg_id);
					}
					//resetar comando
					shared_msg->cmd = 0;
				}
			}
			sem_post(sem_cmd_id);
		}
    }
    else if (pid > 0)
    //PROCESO PADRE LEE LOS COMANDOS
    {
    	printf("Bienvenido a ServiForo!\n\n");
       	//ESPERAR POR UN COMANDO
        char comand[MAX_COMAND] = "";
		while (1)
		{
			//LEER COMANDO
			printf(">> ");
			fgets(comand, MAX_COMAND , stdin);

			if (!strncmp(comand,"exit", 4))
			//COMANDO EXIT
			{
				printf("Apagando el servidor...\n");
				//Esperar clientes
				printf("Hasta pronto!\n");

				//SALIR
				unlinks();
				return 1;
			} 
			else 
			//COMANDO INVALIDO
			{
				printf("Comando inválido!\n");
			}
		}
    }
    else
    //FALLO EL FORK SALGO
    {    
        printf("Error en la ejecución de fork().\n");
        unlinks();
        return 1;
    }

	
    //SALIR
	unlinks();
	return 1;
}