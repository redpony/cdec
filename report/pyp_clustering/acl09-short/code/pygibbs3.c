#include <stdio.h>
#include <math.h>

#define myrand() (double) (((unsigned long) randomMT()) / 4294967296.)

#define W 30114
#define N 831190 
#define KWMAX 1000

#define NLOOPS 1000
#define BURNIN 0
#define SAMPLEFREQ 1

#define ALPHA 0.0 // PYB a
//#define GAMMA 1000000000.0
#define GAMMA .01 // Dirichlet over multinomial P0

double BETA; // CRP alpha (PYB b)
int w[N], z[N]; // words, table assignments
int typecount[W], typetot; //# of tables of each type, total # tables
int usedcount[W];
double ztot[W][KWMAX];
double k; // total # tables
int nactive;

void initialise(void);
void anderson(void);
void fileread(void);

void initialise(void)
{
  int i,j; 

  for (i = 1; i < W; i++) {
    typecount[i] = 0;
    usedcount[i] = 0;
    for (j = 0; j < KWMAX; j++) {
      ztot[i][j] = 0;
    }
  }

}

void anderson(void)  //stochastic Anderson-style initialisation
{
  int i,j, tag;
  double max, totprob, r, runtot;
  double probs[KWMAX];
  int ind, temp;
  
  ztot[w[0]][0] = 1;
  z[0] = 0;
  typecount[w[0]] = 1;
  usedcount[w[0]] = 1;
  k = 1;				       
  typetot = 1;

  for (i = 1; i < N; i++) {
    //    printf("%5d\n", w[i]);
    max = 0; tag = 0; totprob = 0;
    for (j = 0; j < usedcount[w[i]]; j++) {
      probs[j] = ztot[w[i]][j] - ALPHA;
	totprob += probs[j];
    } 
    probs[usedcount[w[i]]] = (ALPHA*k+BETA)*((double) typecount[w[i]]+GAMMA)/((double) typetot+W*GAMMA);
    totprob += probs[usedcount[w[i]]];
    //    printf("%10.6lf\n",totprob);
    r = myrand()*totprob;
    max = probs[0];
    j = 0;
    while (r>max) {
      j++;
      max += probs[j];
    }
    //    printf("%5d\n",j);
    z[i] = j;
    ztot[w[i]][j]++;
    if (ztot[w[i]][j]==1) {
      typecount[w[i]]++;
      usedcount[w[i]]++;
      if (usedcount[w[i]]==KWMAX) {
	printf("Maximum number of tables exceeded!!!\n");
      }
      typetot++;
      k++;
    }
  }
}

void fileread(void) 
{
  int i,j, wt;
  FILE *fileptr;
  
  fileptr = fopen("wsj.dat", "r"); 

  for (i = 1; i < N; i++) {
    fscanf(fileptr, "%d", &wt);
    w[i] = wt-1;
    z[i] = 0;
  }
  printf("Total cases: %10d\n", N);
  fclose(fileptr);
}

main(int argc, char* argv[])
{
  int i,j,loop,run;
  int temp,ind, tag;
  double newprob, WBETA;
  double probs[KWMAX];
  double max, totprob, r;
  int sampcount;
  FILE *fileptr;
  char filename[30];
  double score;

  if (argc < 2) {
    printf("Please provide a value of b\n");
    exit(0);
  }
  BETA = strtol(argv[1]);
  printf("Basic initialising...\n");

   // you can seed with any uint32, but the best are odds in 0..(2^32 - 1)
  seedMT(4157U);

  sprintf(filename,"typecountrecordwsjpeak%0.1f.%0.1f.dat",ALPHA,BETA);
  fileptr = fopen(filename, "w");

  printf("Reading from file...\n");
  fileread();

  printf("Initialising...\n");
  initialise();
    printf("k = %1.0f, typetot = %d\n",k,typetot);

  printf("Finding start state...\n");
  anderson();
  printf("Beginning burnin...\n");
  for (loop = 0; loop < NLOOPS; loop++) {
    for (i = 0; i < N; i++) {
      j = z[i];
      ztot[w[i]][j]--;
      if (ztot[w[i]][j] == 0) {
	if (j==usedcount[w[i]]) {
	  usedcount[w[i]]--;
	}
	typecount[w[i]]--;
	typetot--;
	k--;
      }
      max = 0; tag = 0; totprob = 0;
      for (j = 0; j <= usedcount[w[i]]; j++) {
	if (ztot[w[i]][j] > 0) {
	  probs[j] = ztot[w[i]][j] - ALPHA;
	} else {
	  probs[j] = 0; 
	  if (tag == 0) {
	    probs[j] = (ALPHA*k+BETA)*(((double) typecount[w[i]])+GAMMA)/(((double) typetot)+((double) W)*GAMMA);
	    tag = 1;
	  }
	}
	totprob += probs[j];
      }
      r = myrand()*totprob;
      max = probs[0];
      j = 0;
      while (r>max) {
	j++;
	max += probs[j];
      }
      z[i] = j;
      ztot[w[i]][j]++;
      if (ztot[w[i]][j]==1) {
	if (j == usedcount[w[i]]) {
	  usedcount[w[i]]++;
	  if (usedcount[w[i]]==KWMAX) {
	    printf("Maximum number of tables exceeded!!!\n");
	  }
	}
	typecount[w[i]]++;
	typetot++;
	k++;
      }      
    }
    printf("Completed sample # %5d\n", loop);
    if (k != typetot)  printf("k = %1.0f, typetot = %d\n",k,typetot);
    if (loop >= BURNIN && loop % SAMPLEFREQ == 0) {
      for (i = 0; i < W; i++) {
	fprintf(fileptr," %d", typecount[i]); //print (table?) count for each word type
      }
      fprintf(fileptr,"\n");
    }
  }
  fclose(fileptr);
}
  
