#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xv.h"

static unsigned char CPTbl[] = {
    0x10, 0x17, 0x13, 0x15, 0x09,
    0x08, 0x0a, 0x14, 0x06, 0x05,
    0x16, 0x02, 0x0d, 0x03, 0x01,
    0x04, 0x19, 0x0c, 0x0f, 0x0e,
    0x12, 0x07, 0x0b, 0x18, 0x11, 0x1a
};

void cpcode(char *filename,unsigned char *buff,int code_num)
{
	int i,fd;
  	unsigned char *wp, *cp;
  	unsigned char *CP_KEY = (unsigned char *)"kIUCHIuCHINO";

	if(code_num>=3) return;
	if((fd=open(filename,O_RDONLY))==-1) return;

	lseek(fd,20+16*code_num,SEEK_SET);
	read(fd,buff,16);

  	wp = buff;
  	cp = CP_KEY;

      	for ( i = 0; i < 16; i++ ) {
          	*wp ^= *cp;
          	if ( *wp & 0x80 ) *wp = '\0';
		cp++;
          	wp++;
        	if ( *cp == '\0' ) cp = CP_KEY;
   	}

	for (i=0;i<16;i++) {
		if (buff[i]<'A' || 'Z'<buff[i]){
			buff[i]=0;
			break;
		}
	}
	close(fd);
}


static void GetCPTbl(int nTbl, char *CPcode, int *tbl, CPS *cps)
{
    int i, k, vl1, vl2, len;

    for(i = 0; i < nTbl; i++) {
        tbl[i] = -1;
        cps[i].n = i;
        cps[i].flg = 0;
    }
    len = strlen(CPcode);
    vl1 = nTbl - 1;
    vl2 = len + nTbl % len;
    for(k = 0; k < nTbl; k++) {
        vl1 = CPTbl[CPcode[k % len] - 'A'] + vl1 + vl2;
        if(vl1 >= nTbl) vl1 %= nTbl;
         while(tbl[vl1] != -1) {
            if(k & 01) {
                if(vl1 == 0) vl1 = nTbl;
                vl1--;
            }
            else {
                if(++vl1 >= nTbl) vl1 = 0;
            }
        }
        tbl[vl1] = k;
        vl2++;
    }
    for(i = 0, k = nTbl - 1; i < k; i++, k--) {
        cps[tbl[i]].n = tbl[k];
        cps[tbl[k]].n = tbl[i];
        if((tbl[i] ^ tbl[k]) & 0x01)
            cps[tbl[i]].flg = cps[tbl[k]].flg = 1;
    }
}


CPS *calcCPmask(char *code, int ntbl)
{
    int *tbl;
    int len;
    CPS *cps;

    len = strlen(code);
    tbl = (int *)malloc(sizeof(int) * ntbl);
    cps = (CPS *)malloc(sizeof(CPS) * ntbl);
    if(!tbl || !cps) {
	ErrPopUp ("Error:  No memory!!", "\nOk");
        if(tbl) free(tbl);
        exit(1);
    }
    GetCPTbl(ntbl, code, tbl, cps);
    
    free(tbl);
    return(cps);
}

