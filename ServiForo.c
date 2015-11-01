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
#include <errno.h>

//CONSTANTES CONFIGURABLES
#define MAX_MENSAJES_EN_FORO 100
#define MAX_CLIENTES 20
#define MAX_COMAND 300
#define MAX_LARGO_MENSAJE 200
#define MAX_NAME 10

#define SEM_INSTANCE_NAME "ServiForoInstanceSem"
//NOMBRES DE RECURSOS COMPARTIDOS
#define SHM_PATH         "/ServiForoSharedMemory"
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

//ESTRUCTURAS DEL SERVIDOR
//Array de clientes
char clientes[MAX_CLIENTES][MAX_NAME];
int * cliCount;
int * comingOut;
struct message
{
	char name[MAX_NAME];
	char msg[MAX_LARGO_MENSAJE];
};
struct message messages[MAX_MENSAJES_EN_FORO];
int msgCount = 0;

//DEVUELVE TRUE SI ENCONTRO AL USUSRIO name
int existUser(const char name[MAX_NAME])
{
	int res = 0;
	int i = 0;
	while (i < (*cliCount))
	{
		if (!strcmp(name,clientes[i]))
		{
			res = 1;
			break;
		}
		i++;
	}
	return res;
}

//ELIMINAR AL USUSRIO name
void removeUser(const char name[MAX_NAME])
{
	int i = 0;
	while (i < (*cliCount))
	{
		if (!strcmp(name,clientes[i]))
		{
			break;
		}
		i++;
	}
	while (i < ((*cliCount) -1) )
	{
		strcpy(clientes[i], clientes[i+1]);
	}
	(*cliCount)--;
}

//OBTENER EL RESTO DEL PARAMETRO
void getRest(char param[MAX_COMAND]){
	int i = 0;
	while (i < MAX_COMAND)
	{
		if (param[i] == ' '){
			break;
		}
		i++;
	}
	if (param[i] == ' '){
		int j = 0;
		while (j < (MAX_COMAND-i-1))
		{
			param[j] = param[j+i+1];
			j++;
		}
	}
	else
	{
		strcpy(param, "");
	}
}

//OBTENER EL NOMBRE DEL PARAMETRO
void getName(char param[MAX_COMAND], char name[MAX_NAME]){
	int i = 0;
	while (i < MAX_COMAND)
	{
		if (param[i] == ' '){
			break;
		}
		i++;
	}
	strncpy(name, param, i);
}

//FUNCION QUE LIBERA LOS RECURSOS
void before_return()
{
	shm_unlink(SHM_PATH);
	sem_unlink(SEM_CLI_NAME);
	sem_unlink(SEM_INSTANCE_NAME);
	sem_unlink(SEM_CMD_NAME);
	sem_unlink(SEM_SERVIMSG_NAME);
}

int main()
{
	//SI YA HAY OTRA INSTANCIA EN EJECUCION SALGO
	if (sem_open(SEM_INSTANCE_NAME, O_CREAT, 600, 1) == SEM_FAILED) 
	{
		printf("Ya existe una instancia de ServiForo en ejecución\n\n");
  		before_return();
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
        before_return();
        return -1;
    }
    //Ajustando el tamaño del mapeo al tamaño de la estructura
    ftruncate(shmfd, shared_seg_size);
    //Solicitando el segmento compartido
    shared_msg = (struct shared_data *) mmap(NULL, shared_seg_size, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if (shared_msg == NULL)
    {
        perror("Error en mmap()");
        return -1;
    }
   	//Inicializar estructura
   	shared_msg->CliCmd.num = 0;
   	strcpy(shared_msg->CliCmd.param, "");
   	strcpy(shared_msg->serviMsg, ".");

    //Creando semaforo para clientes
    sem_open(SEM_CLI_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1);
    sem_t * sem_cmd_id;
    sem_t * sem_ServiMsg_id;
    sem_cmd_id = sem_open(SEM_CMD_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1);
    sem_ServiMsg_id = sem_open(SEM_SERVIMSG_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1);

    //COMPARTIR MEMORIA ENTRE PADRE E HIJO
    cliCount = (int *) mmap(NULL, sizeof(int *), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    (*cliCount) = 0;
    comingOut = (int *) mmap(NULL, sizeof(int *), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    (*comingOut) = 0;

	//FORK
    pid_t pid = fork();
    if (pid == 0)
    //PROCESO HIJO ATIENDE A LOS CLIENTES
    {
    	char name[MAX_NAME];
        while (1)
		{
			sem_wait(sem_cmd_id);
			if (shared_msg->CliCmd.num !=0)
			//hay un comando
			{

				if (shared_msg->CliCmd.num == 1)
				//Registrar Cliente
				{
					//obtener nombre
					strcpy(name, shared_msg->CliCmd.param);
					strcpy(shared_msg->CliCmd.param, "");

					if ((*cliCount) + 1 > MAX_CLIENTES)
					{
						//maximo de usuarios superado
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
						(*cliCount)++;
						strcpy(clientes[(*cliCount)-1], name);

						sem_wait(sem_ServiMsg_id);
						strcpy(shared_msg->serviMsg, "ok");
						sem_post(sem_ServiMsg_id);
					}
					//resetar comando
					shared_msg->CliCmd.num = 0;
				} 
				else if (shared_msg->CliCmd.num == 2)
				//BORRAR CLIENTE
				{
					//obtener nombre
					strcpy(name, shared_msg->CliCmd.param);
					strcpy(shared_msg->CliCmd.param, "");
					//quitar de la lista de clientes
					removeUser(name);

					//resetar comando
					shared_msg->CliCmd.num = 0;
				}
				else if (shared_msg->CliCmd.num == 4)
				//ESCRIBIR MENSAJE
				{
					
					if ((*comingOut))
					{
						//Saliendo
						sem_wait(sem_ServiMsg_id);
						strcpy(shared_msg->serviMsg, "No se permiten nuevas operaciones en el foro\n");
						sem_post(sem_ServiMsg_id);
					}
					else
					{
						
						//guardar mensaje
						msgCount++;
						getName(shared_msg->CliCmd.param, name);
						strcpy(messages[msgCount-1].name, name);

						getRest(shared_msg->CliCmd.param);
						strcpy(messages[msgCount-1].msg, shared_msg->CliCmd.param);

						
						//Print debug
						char buffer[100];
						sprintf(buffer, "Se ha creado el mensaje numero %d en el foro", msgCount);
						errno = 0;
						perror(buffer);

						//responder al cliente
						sem_wait(sem_ServiMsg_id);
						strcpy(shared_msg->serviMsg, "ok");
						sem_post(sem_ServiMsg_id);
					}

					//resetar comando
					shared_msg->CliCmd.num = 0;
				}
				else if (shared_msg->CliCmd.num == 3)
				//LISTAR MENSAJES
				{
					
					if ((*comingOut))
					{
						//Saliendo
						sem_wait(sem_ServiMsg_id);
						strcpy(shared_msg->serviMsg, "No se permiten nuevas operaciones en el foro\n");
						sem_post(sem_ServiMsg_id);
					}
					else
					{
						//Imprimir lista
						char msgaux[MAX_LARGO_MENSAJE];
						strcpy(msgaux, "Todos los mensajes\n");
						char buffer[MAX_NAME + 10];
						int i = 0;
						while (i <  msgCount)
						{
							sprintf(buffer, "%d - %s\n", i+1, messages[i].name);
							strcat(msgaux, buffer);
							i++;
						}
						
						//responder al cliente
						sem_wait(sem_ServiMsg_id);
						strcpy(shared_msg->serviMsg, msgaux);
						sem_post(sem_ServiMsg_id);
					}

					//resetar comando
					shared_msg->CliCmd.num = 0;
				}
				else if (shared_msg->CliCmd.num == 5)
				//LEER MENSAJE
				{
					
					if ((*comingOut))
					{
						//Saliendo
						sem_wait(sem_ServiMsg_id);
						strcpy(shared_msg->serviMsg, "No se permiten nuevas operaciones en el foro\n");
						sem_post(sem_ServiMsg_id);
					}
					else
					{
						
						//Obtener numero de mensaje
						int mnum = atoi(shared_msg->CliCmd.param);

						if ((mnum > 0) && (mnum <= msgCount))
						//es un numero valido
						{
							char msgaux[MAX_LARGO_MENSAJE+MAX_NAME+10];
							sprintf(msgaux, "%s: %s\n", messages[mnum-1].name, messages[mnum-1].msg);
							//responder al cliente
							sem_wait(sem_ServiMsg_id);	
							strcpy(shared_msg->serviMsg, msgaux);
							sem_post(sem_ServiMsg_id);	
						}
						else
						{
							sem_wait(sem_ServiMsg_id);	
							strcpy(shared_msg->serviMsg, "Numero fuera de rango\n");
							sem_post(sem_ServiMsg_id);
						}
						
					}

					//resetar comando
					shared_msg->CliCmd.num = 0;
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
		
		printf("Ingrese exit para salir\n");
		while (1)
		{
			//LEER COMANDO
			fgets(comand, MAX_COMAND , stdin);

			if (!strncmp(comand,"exit", 4))
			//COMANDO EXIT
			{
				printf("Esperando que salgan todos los clientes...\n");
				
				//Saliendo igual true
				(*comingOut) = 1;
				//Esperar clientes
				while((*cliCount)>0);

				printf("Hasta pronto!\n");
				//SALIR
				before_return();
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
        before_return();
        return 1;
    }

	
    //SALIR
	//before_return();
	//return 1;
}