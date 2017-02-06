#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "i2cfunc.h"
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <math.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#define I2C_ADDR 0x40
#define I2C_READ_T 0xe3
#define I2C_READ_H 0xf5
#define I2CBUS 1
#define DELAY	5
#define STR_LEN 	 	64      /* ->taille des chaines de caracteres */
#define NON_ZERO_VALUE		8  	/* ->une valeur non nulle pour la generation de la clef */
#define GPIO60 "/sys/class/gpio/gpio60"
#define GPIO_EXPORT "/sys/class/gpio/export"
typedef struct EtatMoteur
{
  double 	t;	/* ->température         */
  double 	h;	/* ->humidité            */
  int  u;
} EtatMoteur;
/* variables globales */
EtatMoteur 	*em;
double 		temps = 0.0;	//temps
double 		Te; 		//period
double    T,H;
FILE *fOut;
char gpioDir [256];
char gpioVal [256];
double I2C_BBB_Read_Temp();
double I2C_BBB_Read_RH();
void sighandler( int ); 

double I2C_BBB_Read_Temp()
	{  /*Ce module ce charge de lire les valeurs du capteur de temperature via le protocole I2C.  */
	unsigned char inBuffer[3];
	long tData ;
	double temperature;
	int handle;
	handle=i2c_open(I2CBUS,I2C_ADDR);
	i2c_write_byte(handle,I2C_READ_T);
	delay_ms(100);
	i2c_read(handle,inBuffer,3);
	inBuffer[1]=inBuffer[1] & 0xfc;
	inBuffer[1]=inBuffer[1]>>2;
	inBuffer[1]=inBuffer[1] & 0x3f;
	tData = inBuffer[0]*256 + inBuffer[1];
	temperature = -46.85 +175.72*((double)(tData)/65536.0);
	tData = (int)(temperature);
	i2c_close(handle);
	return(temperature);
	}
	
double I2C_BBB_Read_RH()
	{  /*Ce module ce charge de lire les valeurs du capteur d'humidite via le protocole I2C.  */
	unsigned char inBuffer[3];
	long tData ; 
	double humi;
	int handle;
	handle=i2c_open(I2CBUS,I2C_ADDR); 
	i2c_write_byte(handle,I2C_READ_H);
	delay_ms(100);
	i2c_read(handle,inBuffer,3);
	inBuffer[1]&= 0xfc;
  tData = inBuffer[0] * 256 + inBuffer[1];
  humi = -6.0 + 125.0 * ((double)((unsigned int)(tData)) / 65536.0);
	tData=(int)(humi);
	i2c_close(handle);
	return(humi);
	}
	
void sighandler( int signal )
{   /*Envoie un signal quand on depasse une valeur critique pour T et H */
  if( signal == SIGALRM )   
  {
    
    T=I2C_BBB_Read_Temp();
	  H=I2C_BBB_Read_RH();  
    em->t=T;
    em->h=H;
    
    fOut=fopen(gpioVal,"w");
    fprintf(fOut,"%d",em->u);
    fclose(fOut);
    printf("%lf\t%lf\t%lf\t%d\n ", temps, em->t, em->h, em->u );
    
  };
  temps+=Te;  // refresh
}	

int main( int argc, char *argv[] )
{  // fil gris p9 pin 12 
  char  		cmd[STR_LEN];   /* ->construction de la commande   */			
  struct sigaction sa;	
  struct itimerval 	period;
  key_t shm_key_etat;	/* ->clef sur la zone partagee de l'etat moteur    */
  key_t shm_key_etat1;
  sigset_t 	blocked;
  int   shm_id_etat;	/* ->ID de la zone partagee sur l'etat             */
  int   shm_id_etat1;

  memset(gpioDir,0,256);
  memset(gpioVal,0,256);
  fOut=fopen(GPIO_EXPORT,"w");
  fprintf(fOut,"%d",60);
  fclose(fOut);
  sprintf(gpioDir,"%s/direction",GPIO60);
  fOut=fopen(gpioDir,"w");
  fprintf(fOut,"%s","out");
  fclose(fOut);
  sprintf(gpioVal,"%s/value",GPIO60);
    
  if( argc != 3 ) 
  {
    printf("%s <Nom Zone Moteur>\t<periode>\n", argv[0] );
    return( 0 );
  };
  if( sscanf(argv[2],"%lf", &Te) == 0 )
  {
    printf("%s <periode>\n", argv[0] );
    return( 0 );
  };
 
  /* tentative d'acces a la zone partagee */
  /* on commence par creer un fichier */
  /* vide de meme nom que la zone :   */
  /*LA ZONE MOTEUR */
  sprintf(cmd,"touch %s",argv[1]);
  system(cmd);
  // on peut maintenant construire la clef pour la commande : 
  if( (shm_key_etat1 = ftok(argv[1],(int)(NON_ZERO_VALUE))) < 0 )
  {
    fprintf(stderr,"ERREUR : appel a ftok() depuis main()...\n");
    fprintf(stderr,"         code d'erreur = %d\n", errno );
    exit( errno );
  };
  // pour ensuite construire la zone partagee 
  if( (shm_id_etat1 = shmget( shm_key_etat1, sizeof(double), IPC_CREAT )) < 0 )
  {
    fprintf(stderr,"ERREUR : appel a shmget() depuis main()...\n");
    fprintf(stderr,"         code d'erreur = %d\n", errno );
    exit( errno );
  };
  em = (EtatMoteur *)(shmat(shm_id_etat1, NULL, 0));
  if( em == NULL ) 
  {
    fprintf(stderr,"ERREUR : appel a shmat() depuis main()...\n");
    fprintf(stderr,"         code d'erreur = %d (%s) \n", errno, strerror(errno));
  };
  
  period.it_interval.tv_sec  	= (int)(Te);
  period.it_interval.tv_usec 	= (int)((double)((Te - (int)(Te))) * 1e6);
  period.it_value.tv_sec    	= period.it_interval.tv_sec;
  period.it_value.tv_usec    	= period.it_interval.tv_usec;
  /* init pour la routine d'interception */
  	
  memset( &sa, 0 , sizeof( sa ) ); /* -> NE PAS OUBLIER */
  sigemptyset( &blocked );         /* ->liste vide pour les signaux a bloquer */
  sa.sa_handler = sighandler;
  sa.sa_mask    = blocked;
  /* installation de la routine d'interception */
  sigaction( SIGALRM, &sa, NULL ) ; /*->la routine est associee a l'alarme cyclique */
  /* demarrage du timer */
  setitimer(ITIMER_REAL, &period, NULL );
  
   do
  {
    pause();
  }
  while( 1 );
  /* liberation des zones partagees */
  /* (1) on doit "detacher" la zone : */
  shmdt((void *)(em) );
  
  return( 0 );
}
