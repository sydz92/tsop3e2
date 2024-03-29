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
#include <time.h>

#define MAX_COMAND 300
#define MAX_LARGO_MENSAJE 200
#define MAX_NAME 50

//NOMBRES DE RECURSOS COMPARTIDOS
#define shm_openATH "/ServiForoSharedMemory"
#define SEM_CLI_NAME "ServiForoCliSem"
#define SEM_CMD_NAME "ServiForoCmd"
#define SEM_SERVIMSG_NAME "ServiForoServiMsg"

//ESTRUCTURA DE DATOS COMPARTIDA
struct cmd {
	int num;
	/*
	Comandos:
	1 - Registrar Cliente
	2 - Borrar cliente
	3 - List
	4 - Write
	5 - Read
	*/ 
	char param[MAX_COMAND];
};
struct shared_data {
    struct cmd CliCmd;
    char serviMsg[MAX_LARGO_MENSAJE];
};

//FILE DESCRIPTORS
sem_t * sem_cli_id;
sem_t * sem_cmd_id;
sem_t * sem_ServiMsg_id;
int shmfd;

//OBTENER EL RESTO DEL COMANDO
void getRest(char comand[MAX_COMAND]){
	int i = 0;
	while (i < MAX_COMAND)
	{
		if (comand[i] == ' '){
			break;
		}
		i++;
	}
	if (comand[i] == ' '){
		int j = 0;
		while (j < (MAX_COMAND-i-1))
		{
			comand[j] = comand[j+i+1];
			j++;
		}
	}
	else
	{
		strcpy(comand, "");
	}
}

//FUNCION QUE LIBERA LOS RECURSOS
void before_return()
{
	/*shm_unlink(SHM_PATH);
	sem_unlink(SEM_CLI_NAME);
	sem_unlink(SEM_CMD_NAME);
	sem_unlink(SEM_SERVIMSG_NAME);*/
	close(shmfd);
	sem_close(sem_cli_id);
	sem_close(sem_cmd_id);
	sem_close(sem_ServiMsg_id);
}

int main(int argc, char * argv[])
{
	//COMPROBAR QUE EL SERVIDOR ESTA LEVANTADO
	sem_cli_id = sem_open(SEM_CLI_NAME, 0, 600, 1);
    sem_cmd_id = sem_open(SEM_CMD_NAME, 0, 600, 1);
    sem_ServiMsg_id = sem_open(SEM_SERVIMSG_NAME, 0, 600, 1);
	if ((sem_cli_id == SEM_FAILED)  || (sem_cmd_id == SEM_FAILED) || (sem_ServiMsg_id == SEM_FAILED))
	{
		printf("Error, no exite una instancia de ServiForo en ejecución\n\n");
  		before_return();
  		return -1;
	}

	//Tama;o de la estructura
    int shared_seg_size = (1 * sizeof(struct shared_data)); 
    //Estructura compartida
    struct shared_data *shared_msg;
     //Creando el objeto de memoria compartido 
    shmfd = shm_open(SHM_PATH, O_RDWR, S_IRWXU | S_IRWXG);
    if (shmfd < 0)
    {
        perror("Error en shm_open()");
        before_return();
  		return -1;
    }
    //Ajustando el tama;o del mapeo al tama;o de la estructura
    ftruncate(shmfd, shared_seg_size);
    //Solicitando el segmento compartido    
	shared_msg = (struct shared_data *)mmap(NULL, shared_seg_size, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if (shared_msg == NULL)
    {
       	printf("Error, no exite una instancia de ServiForo en ejecución\n\n");
        before_return();
        return -1;
    }

	//NOMBRE DE USUARIO
	char name[MAX_NAME];
	if (argc == 1)
	{
		printf("Debe ingresar un nombre de usuario\n\n");
		before_return();
		return -1;
	}
	else 
	{		
		strcpy(name, argv[1]);
		//obtener semaforo de cliente
		sem_wait(sem_cli_id);
		
		//Llamar comando Registrar Cliente
		sem_wait(sem_cmd_id);

		strcpy(shared_msg->CliCmd.param, name);
		shared_msg->CliCmd.num=1;

		sem_post(sem_cmd_id);

		//Esperar por respuesta
		while(1)
		{
			sem_wait(sem_ServiMsg_id);
			if (strcmp(shared_msg->serviMsg,"."))
			//el servidor me reopondio
			{
				if (!strcmp(shared_msg->serviMsg,"ok"))
				//exito
				{
					strcpy(shared_msg->serviMsg, ".");
					sem_post(sem_ServiMsg_id);
					break;	
				}
				else 
				//algo fallo
				{
					printf("%s", shared_msg->serviMsg);
					strcpy(shared_msg->serviMsg, ".");
					
					sem_post(sem_ServiMsg_id);
					sem_post(sem_cli_id);
					before_return();
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
			printf("Saliendo...\n");
			
			sem_wait(sem_cli_id);
			//Llamar des-registrar Cliente
			sem_wait(sem_cmd_id);

			strcpy(shared_msg->CliCmd.param, name);
			shared_msg->CliCmd.num=2;

			sem_post(sem_cmd_id);
			sem_post(sem_cli_id);
			
			//SALIR
			printf("Hasta pronto!\n");
			before_return();
			return 1;
		}
		else if (!strncmp(comand,"write", 5))
		{
			//obtener semaforo de cliente
			sem_wait(sem_cli_id);
			
			//Llamar comando write
			sem_wait(sem_cmd_id);
			//Agregar el nombre del cliente
			strcpy(shared_msg->CliCmd.param, name);

			//agregar mensaje
			getRest(comand);
			strcat(shared_msg->CliCmd.param, " ");
			strcat(shared_msg->CliCmd.param, comand);

			shared_msg->CliCmd.num=4;

			sem_post(sem_cmd_id);

			//Esperar por respuesta
			while(1)
			{
				sem_wait(sem_ServiMsg_id);
				if (strcmp(shared_msg->serviMsg,"."))
				//el servidor me reopondio
				{
					if (!strcmp(shared_msg->serviMsg,"ok"))
					//exito
					{
						strcpy(shared_msg->serviMsg, ".");
						sem_post(sem_ServiMsg_id);
						break;	
					}
					else 
					//algo fallo
					{
						printf("%s", shared_msg->serviMsg);
						strcpy(shared_msg->serviMsg, ".");
						sem_post(sem_ServiMsg_id);
						break;
					}
				}
				sem_post(sem_ServiMsg_id);
			}
			//Esperar para terminar el comando
			printf("Esperando entre 1 a 5 segundos...\n");
			sleep((rand() % 5)+1);
			printf("Espera terminada\n");

			sem_post(sem_cli_id);
		}
		else if (!strncmp(comand,"list", 4))
		{
			//obtener semaforo de cliente
			sem_wait(sem_cli_id);
			
			//Llamar comando list
			sem_wait(sem_cmd_id);
			
			//indicar comnado
			shared_msg->CliCmd.num=3;

			sem_post(sem_cmd_id);

			//Esperar por respuesta
			while(1)
			{
				sem_wait(sem_ServiMsg_id);
				if (strcmp(shared_msg->serviMsg,"."))
				//el servidor me reopondio
				{
					printf("%s", shared_msg->serviMsg);
					strcpy(shared_msg->serviMsg, ".");
					sem_post(sem_ServiMsg_id);
					break;
				}
				sem_post(sem_ServiMsg_id);
			}
			//Esperar para terminar el comando
			printf("Esperando entre 1 a 5 segundos...\n");
			sleep((rand() % 5)+1);
			printf("Espera terminada\n");
			
			sem_post(sem_cli_id);
		}
		else if (!strncmp(comand,"read", 4))
		{
			//obtener semaforo de cliente
			sem_wait(sem_cli_id);
			
			//Llamar comando read
			sem_wait(sem_cmd_id);

			//agregar mensaje
			getRest(comand);
			strcpy(shared_msg->CliCmd.param, comand);

			shared_msg->CliCmd.num=5;

			sem_post(sem_cmd_id);

			//Esperar por respuesta
			while(1)
			{
				sem_wait(sem_ServiMsg_id);
				if (strcmp(shared_msg->serviMsg,"."))
				//el servidor me reopondio
				{
					printf("%s", shared_msg->serviMsg);
					strcpy(shared_msg->serviMsg, ".");
					sem_post(sem_ServiMsg_id);
					break;
				}
				sem_post(sem_ServiMsg_id);
			}
			//Esperar para terminar el comando
			printf("Esperando entre 1 a 5 segundos...\n");
			sleep((rand() % 5)+1);
			printf("Espera terminada\n");

			sem_post(sem_cli_id);
		} 
		//COMANDO INVALIDO
		else 
		{
			printf("Comando inválido!\n");
		}
	}

	//SALIR
	before_return();
	return 1;
}