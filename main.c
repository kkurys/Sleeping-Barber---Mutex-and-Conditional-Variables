#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <time.h>
#include <pthread.h>
#include <math.h>

/* BY KAMIL KURYŚ & MACIEJ OWERCZUK
   SLEEPING BARBER PROBLEM USING CONDITION VARIABLES
   BIAŁYSTOK, 2016 */

// czy jest klient / czy fryzjer jest gotowy / czy fryzjer strzyze

pthread_cond_t clientsAvailable = PTHREAD_COND_INITIALIZER;
pthread_cond_t barberReady = PTHREAD_COND_INITIALIZER;
// potrzebne aby tylko jeden klient naraz mogl byc strzyzony
pthread_cond_t barberCutting = PTHREAD_COND_INITIALIZER;
// 
// blokada dostepu do krzesel / blokada dostepu do fryzjera / blokada stanu bycia strzyzonym
pthread_mutex_t accessChairs = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t accessBarber = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t gettingCut = PTHREAD_MUTEX_INITIALIZER;

int numberOfWaitingClients = 0;
int numberOfChairs;
int totalNumberOfClients = 0;
int numberOfResignedClients = 0;
int IsDebug = 0;
int currentlyServedClient = 0;
int cutting = 0;

typedef struct Pos_Struct
{
	int id;
	pthread_cond_t turn;
	struct Pos_Struct *next;
}Position;


void DoCut(Position *client)
{
	int i = rand()%4;
// zaczyna sie strzyzenie
	pthread_mutex_lock(&gettingCut);
	cutting = 1;
	sleep(i);
	cutting = 0;
	pthread_cond_signal(&barberCutting);
// sygnalizujemy zakonczenie strzyzenia
	pthread_mutex_unlock(&gettingCut);
}
void GetCut()
{
// czekamy na ostrzyzenie
	pthread_mutex_lock(&gettingCut);
	while (cutting)
	{
		pthread_cond_wait(&barberCutting, &gettingCut);
	}
	pthread_mutex_unlock(&gettingCut);
}

// barberQueue - kolejka w poczekalni; resignedClients - lista zrezygnowanych klientow
Position *barberQueue = NULL;
Position *resignedClients = NULL;
// wypisywanie kolejki
void PrintQueue(Position *start)
{
	Position *tmp = NULL;
	if (start == NULL) 
	{	
		return;
	}
	else 
	{
		tmp = start;
		while (tmp != NULL)
		{
			printf("%d ", tmp->id);
			tmp = tmp->next;
		}
	}
	printf("\n");
}

// dodawanie klienta do kolejki
Position *AddNewClientToQueue(Position **start, int id)
{
	Position *new = malloc(sizeof(Position));
	new->id = id;
	new->next = NULL;
	pthread_cond_init(&new->turn, NULL);

	if (*start == NULL)
	{
		(*start) = new;
	}
	else 
	{
		Position *tmp = *start;
		while (tmp->next != NULL)
		{
			tmp = tmp->next;
		}
		tmp->next = new;
	}
	return new;
}

// wpuszczanie klienta do strzyzenia czyli "pobieranie" go z poczekalni
Position *AllowClientIn()
{
	Position *tmp = barberQueue;
	barberQueue = barberQueue->next;
	return tmp;

}

// metoda do wypisywania list przy parametrze -debug
void PrintDebug()
{
	printf("Waiting: ");	
	PrintQueue(barberQueue);
	printf("Resigned: ");	
	PrintQueue(resignedClients);
	printf("\n\n");
}

// metoda do wypisywania podstawowych informacji: liczbie zrezygnowanych klientow, stanie poczekalni oraz aktualnie obslugiwanym kliencie
void PrintStats()
{
	if (currentlyServedClient != 0)
	{
		printf("Res: %d wRoom: %d/%d In: %d\n", numberOfResignedClients, numberOfWaitingClients, numberOfChairs, currentlyServedClient);
	}
	else
	{
		printf("Res: %d wRoom: %d/%d In: -\n", numberOfResignedClients, numberOfWaitingClients, numberOfChairs);
	}
	if (IsDebug)
	{
		PrintDebug();
	}
}


void Customer()
{
	int id;
// klient wchodzi do poczekalni i zmienia stan liczby wolnych krzesel w poczekalni, wiec musi zablokowac do nich dostep
	pthread_mutex_lock(&accessChairs);
// kolejnym klientom nadawane jest ID rowne liczbie wszystkich klientow (pierwszy - 1, drugi - 2, itd..)
	totalNumberOfClients++;
	id = totalNumberOfClients;
// jezeli sa miejsca w poczekalni
	if (numberOfWaitingClients < numberOfChairs)
	{
// to doliczamy kolejnego ktory czeka i dokladamy go do listy i wypisujemy zmiany
		numberOfWaitingClients++;
		Position *client = AddNewClientToQueue(&barberQueue, totalNumberOfClients);
		printf("New client got into lobby!\n");
		PrintStats();
// informujemy, ze czeka klient
		pthread_cond_signal(&clientsAvailable);
// zmiany w stanie krzesel juz sie skonczyly wiec mozemy odblokowac dostep
		pthread_mutex_unlock(&accessChairs);

// czekamy na nasza kolej fryzjera
		pthread_mutex_lock(&accessBarber);
		while (currentlyServedClient != id)
		{
			pthread_cond_wait(&client->turn, &accessBarber);
		}
		pthread_mutex_unlock(&accessBarber);
		GetCut();
	}
	else 
	{
// dodajemy zrezygnowanego klienta do listy, zliczamy i wypisujemy zmiany
		AddNewClientToQueue(&resignedClients, totalNumberOfClients);
		numberOfResignedClients++;
		printf("New client couldn't get into lobby!\n");
		PrintStats();
// rowniez mozemy odblokowac stan krzesel
		pthread_mutex_unlock(&accessChairs);
	}
}
void Barber()
{
	while(1)
	{
// sprawdzamy poczekalnie i czekamy na klienta
		pthread_mutex_lock(&accessChairs);
		while (numberOfWaitingClients == 0)
		{
			currentlyServedClient = 0;
			pthread_cond_wait(&clientsAvailable, &accessChairs);
		}
		numberOfWaitingClients--;
		Position *client= AllowClientIn(barberQueue);
		currentlyServedClient = client->id;
		printf("New client for the cut!\n");
		PrintStats();
// umozliwiamy zmiane stanu poczekalni
		pthread_mutex_unlock(&accessChairs);
// sygnalizujemy gotowosc do strzyzenia
		pthread_cond_signal(&client->turn);
// wykonujemy strzyzenie
		DoCut(client);
	}				
}

int main(int argc, char *argv[])
{
	int status = 0;
	pthread_t id1, id2;
	srand(time(NULL));
	if (argc < 2)
	{
		printf("N chairs expected!\n");
		exit(-1);
	}
	numberOfChairs = atoi(argv[1]);
	if (argc == 3)
	{
		if ((strncmp(argv[2], "-debug", 6) == 0))
		{
			IsDebug = 1;
		}
	}

	status = pthread_create(&id1, NULL, (void*)Barber, NULL);
	if (status!=0)
	{
		printf("Barber thread couldn't start!'\n");
		exit(status);
	}
	while (1)
	{
		int k = rand()%3;
		sleep(k);
		status = pthread_create(&id2, NULL, (void*)Customer, NULL);
		if (status != 0)
		{
			printf("Customer thread couldn't start!\n");
			exit(status);
		}
	}

	pthread_join(id1, NULL);
	return 0;
}
