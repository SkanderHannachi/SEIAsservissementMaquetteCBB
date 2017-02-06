/*Ce programme se charge de lire les informations sur letat du moteur et renvoie la commande dans une zone partagée. */


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
#define NON_ZERO_VALUE		8
#define STR_LEN 	 	64

typedef struct HBITV
{
  double 	  Tv;	       
  double 	  Tolerance;	
  unsigned short    c;
} HBITV;
HBITV *hb;

int main( int argc, char *argv[] )
{
  char  cmd[STR_LEN];		
  key_t shm_key_etat1;	/* ->clef sur la zone partagee de l'etat moteur    */
  int   shm_id_etat1;	/* ->ID de la zone partagee sur l'etat             */
  double var1,var2;

  sprintf(cmd,"touch %s",argv[1]);
  system(cmd);
  /* on peut maintenant construire la clef pour la commande : */
  if( (shm_key_etat1 = ftok(argv[1],(int)(NON_ZERO_VALUE))) < 0 )
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
  
  
  if( argc != 4 ) 
  {
    printf("%s <Nom Zone HBITV>\t<Temperature>\t<Tolerance>\n", argv[0] );
    return( 0 );
  };
  if( sscanf(argv[argc-1],"%lf", &var1) == 0 )
  {
    printf("%s <Tolerance>\n", argv[0] );
    return( 0 );
  };
  
  if( sscanf(argv[argc-2],"%lf", &var2) == 0 )
  {
    printf("%s <Temperature>\n", argv[0] );
    return( 0 );
  };
  hb->Tv=var2;
  hb->Tolerance=var1;
  hb->c=1;
  
  shmdt((void *)(hb) );
  
  return 0;
  
}
  
