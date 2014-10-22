#include "ramp.h"
#include <stdlib.h>
#include <unistd.h>
#include <zlib.h>
#include "gdfontl.h"

char *err;

int gradient[768];

int bluetr = gdTrueColorAlpha(0, 0, 255, gdAlphaTransparent / 2);
int redtr = gdTrueColorAlpha(255, 0, 0, gdAlphaTransparent / 2);
int greentr = gdTrueColorAlpha(0, 255, 0, gdAlphaTransparent / 2);
int greytr = gdTrueColorAlpha(155, 155, 155, gdAlphaTransparent / 2);
int white = gdTrueColorAlpha(255, 255, 255, 0);

// Image generation part
void generate_gradient(gdImagePtr im) {
  int c;
  for (c=0;c<=255;c++) {
    gradient[c]=  gdTrueColor(c,0,0);
    gradient[c+256]= gdTrueColor(255,c,0);
    gradient[c+512]= gdTrueColor(255,255,c);
  }
}

int write_band_Image(char* filename, int width, int height, float **data) //, int *gradient, char* title)
{
  
  int j,k;
  int code = 0;
  gdImagePtr im;
  FILE *out;
  //  int temp;
  //char *err;
  //int brect[8];
  //double sz = 80.; // Font size

  im = gdImageCreateTrueColor(width,height );

  gdImageFilledRectangle(im,0,0,width,height,white);
  
  out = fopen(filename, "wb");
  if (!out) {
    fprintf(stderr, "Can't create output file\n");
    return 1;
    }
	
  generate_gradient(im);
  //  add_scale(im,greytr,chromato);

  for (k=0;k<width;k++) {

    for(j=0;j<height;j++){

      gdImageSetPixel(im,k,j,gradient[(int) (*data)[k]]);
    }
  }

  gdImagePngEx(im, out,3); // Compression of the PNG (higher level <=> smaller file, but long calculation time)
  fclose(out);
  gdImageDestroy(im);
  gdFontCacheShutdown();
  return code;
}

void linear_interpolation(float **data, int c, int d) {
  int pos;
  // y=ax+b  a=(x(d)-x(c))/(d-c)
  // b=x(d)-a*d
  float a = ((*data)[d]-(*data)[c])/(d-c);
  float b = (*data)[d]-a*d;
  for (pos=c;pos<d;pos++) {
    (*data)[pos] = a*pos+b;
  }
}

void normalize(float **data, int lenght)
{
  int i;
  float temp;
  int c=0;
  int range[2];
  int d=0;
  range[0]=(*data)[0];
  range[1]=(*data)[0];
  for(c=0; c<lenght; c++) {
    if(range[1] < (*data)[c]) {
      range[1] = (*data)[c];
    } 
    if ((*data)[c]==-1) {
      d=c;
      while(d<lenght) {
	if ((*data)[d]!=-1) {
	  linear_interpolation(data,c-1,d);
	  break;
	}
	d++;
      }
    }
    if (range[0] > (*data)[c]) {
      range[0] = (*data)[c];  
    }   
    
  }
  float ecart = (float) range[1]-((float) range[0]);      
    
  for (i=0 ; i< lenght ; i++) {
    if (ecart==0) {
      (*data)[i]=0;
    } else {
    temp = (((float) (*data)[i])-((float) range[0]))/(ecart); 
    (*data)[i] = 767*temp ;
    }
    if ((*data)[i]>767) (*data)[i]=767;
    if ((*data)[i]<0) (*data)[i]=0;
    }
  
}

typedef struct {
    int size;
    double * xval;
    double * yval;
} spectStrct;

struct ScanHeaderStruct scan_header;
struct RunHeaderStruct runHeader;
struct ScanHeaderStruct scanHeader;

int main(int argc, char *argv[]) {
  RAMPFILE *mzXML_file;
  ramp_fileoffset_t indexOffset;
  int iLastScan;
  ramp_fileoffset_t *pScanIndex;

  RAMPREAL *pPeaks;
  float *eic_data;
  short int polarity;
  char filename[FILENAME_MAX]; // Band png output name

  if (argc<3) {
    fprintf(stderr,"Usage: %s file minimal_mass maximal_mass polarity\n",argv[0]);
    return(1);
  }

  if (strcmp(argv[4],"+")==0) {
	  polarity = 1;
  } else if (strcmp(argv[4],"-")==0) {
	  polarity = 0;
  } else {
	  fprintf(stderr, "Invalid polarity, use + or -\n");
	  return 1;
  }

  mzXML_file=rampOpenFile(argv[1]);

  indexOffset = getIndexOffset (mzXML_file); /* read the index offset */
  pScanIndex = readIndex(mzXML_file, indexOffset, &iLastScan); /* read the scan index into a vector and get LastScan */

  readRunHeader(mzXML_file, pScanIndex, &runHeader, iLastScan);
   eic_data=(float *) malloc(((int) runHeader.dEndTime+1)*sizeof(float));

  for (int scan=0;scan<runHeader.dEndTime;scan++) {
    eic_data[scan]=-1;
  }
  
  for (int scan=0;scan<iLastScan;scan++) {
    readHeader(mzXML_file, pScanIndex[scan], &scanHeader);

    if (scanHeader.msLevel==1) {

      pPeaks= readPeaks(mzXML_file,pScanIndex[scan]);
      int n=0;
      float count=0;

      while(n<scanHeader.peaksCount*2) {

	if ((float) pPeaks[n]>atof(argv[2]) && (float) pPeaks[n]<atof(argv[3])) {
	  count=count+pPeaks[n+1];
	}
	n=n+2;
      }

      if (scanHeader.polarity == polarity) {
	eic_data[(int) scanHeader.retentionTime]=count;
      }

    }
  }
  snprintf(filename,sizeof(filename),"band_%s-%s-%s.png",argv[4],argv[2],argv[3]);
  normalize(&eic_data,(int) runHeader.dEndTime);
  write_band_Image(filename, (int) runHeader.dEndTime, 100, &eic_data);
  return(0);
}
