#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
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

//struct
typedef struct HBITV   //pour la zone HBITV
{
  double 	  Tv;	       
  double 	  Tolerance;	
  unsigned short  c;
} HBITV;
typedef struct EtatMoteur
{
  double 	t;	/* ->température         */
  double 	h;	/* ->humidité            */
  unsigned short  u;
} EtatMoteur;

EtatMoteur 	*em;/* ->etat du moteur mis a jour dans la memoire partagee */
HBITV *hb;
double 		  temps = 0.0;		/* ->"date"                                             */
double 		  Te; 		        /* ->periode d'echantillonnage                          */
double      tmin,tmax;
void sighandler( int ); 
void sighandler( int signal )
{
  if( signal == SIGALRM )
  {
    tmin=(hb->Tv)*(1.0-(hb->Tolerance)*0.01);
    tmax=(hb->Tv)*(1.0+(hb->Tolerance)*0.01);
    if (em->t<tmin) 
      em->u=hb->c;
    else 
      em->u=0;
    
    printf("%lf\t%lf\t%lf\t%d\t%d\n", temps, tmin, hb->Tv, hb->c ,em->u);
    
  };
  //refresh
  temps+=Te;}


int main( int argc, char *argv[] )
{
  char  		cmd[STR_LEN];		
  struct sigaction sa;	
  struct itimerval 	period;
  key_t shm_key_etat;	/* ->clef sur la zone partagee de l'etat moteur    */
  int   shm_id_etat;	/* ->ID de la zone partagee sur l'etat             */
  key_t shm_key_etat1;	/* ->clef sur la zone partagee de l'etat moteur    */
  int   shm_id_etat1;	/* ->ID de la zone partagee sur l'etat             */
  		
  sigset_t 	blocked;	
  
  sigaction( SIGALRM, &sa, NULL );
  
  if( argc != 4 ) 
  {
    printf("%s <Nom Zone Moteur>\t<Nom Zone HBITV>\t<periode>\n", argv[0] );
    return( 0 );
  };
  if( sscanf(argv[argc-1],"%lf", &Te) == 0 )
  {
    printf("%s <periode>\n", argv[0] );
    return( 0 );
  };

  /* tentative d'acces a la zone partagee */
  /* on commence par creer un fichier */
  /* vide de meme nom que la zone :   */
  /*Zone HBITV */
    sprintf(cmd,"touch %s",argv[2]);
    system(cmd);
  /* on peut maintenant construire la clef pour la commande : */
  if( (shm_key_etat1 = ftok(argv[2],(int)(NON_ZERO_VALUE))) < 0 )
  {
    fprintf(stderr,"ERREUR : appel a ftok() depuis main()...\n");
    fprintf(stderr,"         code d'erreur = %d\n", errno );
    exit( errno );
  };
  /* pour ensuite construire la zone partagee */
  if( (shm_id_etat1 = shmget( shm_key_etat1, sizeof(double), IPC_CREAT )) < 0 )
  {
    fprintf(stderr,"ERREUR : appel a shmget() depuis main()...\n");
    fprintf(stderr,"         code d'erreur = %d\n", errno );
    exit( errno );
  };
  hb = (HBITV *)(shmat(shm_id_etat1, NULL, 0));
  if( hb == NULL ) 
  {
    fprintf(stderr,"ERREUR : appel a shmat() depuis main()...\n");
    fprintf(stderr,"         code d'erreur = %d (%s) \n", errno, strerror(errno));
  };
  
  
  ////////////////////////////////////////////////////////////////////// ZONE MOTEUR
  sprintf(cmd,"touch %s",argv[1]);
  system(cmd);
  
  if( (shm_key_etat = ftok(argv[1],(int)(NON_ZERO_VALUE))) < 0 )
  {
    fprintf(stderr,"ERREUR : appel a ftok() depuis main()...\n");
    fprintf(stderr,"         code d'erreur = %d\n", errno );
    exit( errno );
  };
  /* pour ensuite construire la zone partagee */
  if( (shm_id_etat = shmget( shm_key_etat, sizeof(double), IPC_CREAT )) < 0 )
  {
    fprintf(stderr,"ERREUR : appel a shmget() depuis main()...\n");
    fprintf(stderr,"         code d'erreur = %d\n", errno );
    exit( errno );
  };
  em = (EtatMoteur *)(shmat(shm_id_etat, NULL, 0));
  if( em == NULL ) 
  {
    fprintf(stderr,"ERREUR : appel a shmat() depuis main()...\n");
    fprintf(stderr,"         code d'erreur = %d (%s) \n", errno, strerror(errno));
  };
  /*echantillonnage */
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
  sigaction( SIGALRM, &sa, NULL ) ; //associer la routine à l'alarme cyclique
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
  shmdt((void *)(hb) );
  /* fin */
  return( 0 );
}
