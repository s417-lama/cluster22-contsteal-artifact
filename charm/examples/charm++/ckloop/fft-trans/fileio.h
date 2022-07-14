#include "fftmacro.h"
void readCommFile(fft_complex *data, char *filename) {
  FILE *pFile;
  if(!(pFile = fopen (filename,"r"))) {
    printf("File open failed\n");
    return;
  }

  int l = 0;
#ifdef SINGLE_PRECISION
  while(fscanf (pFile, "%f %f", &data[l][0], &data[l][1]) != EOF) {l++;}
#else
  while(fscanf (pFile, "%lf %lf", &data[l][0], &data[l][1]) != EOF) {l++;}
#endif

  fclose(pFile);
}

void writeCommFile(int n, fft_complex *data, char *filename) {
  FILE *pFile;
  if(!(pFile = fopen (filename, "w"))) {
    printf("File open for write failed\n");
    return;
  }

  for(int l = 0; l < n; l++)

    fprintf(pFile, "%.24f %.24f\n", data[l][0], data[l][1]);
  fclose(pFile);
}
