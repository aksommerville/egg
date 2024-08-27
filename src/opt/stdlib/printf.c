#include "egg-stdlib.h"
#include "egg/egg.h"
#include <stdarg.h>
#include <limits.h>

void *stdout=0;
void *stderr=0;

/* vsnprintf
 */
 
int vsnprintf(char *dst,int dsta,const char *fmt,va_list vargs) {
  int dstc=0;
  if (fmt) {
    while (*fmt) {
    
      // Verbatim literals.
      if (*fmt!='%') {
        if (dstc<dsta) dst[dstc]=*fmt;
        dstc++;
        fmt++;
        continue;
      }
      fmt++;
      if (!*fmt) break; // Trailing '%', dunno what to make of that so ignore it.
      
      // Escaped '%'.
      if (*fmt=='%') {
        if (dstc<dsta) dst[dstc]='%';
        dstc++;
        fmt++;
        continue;
      }
      
      /* Flags.
       */
      const char *fmtstart=fmt;
      int prefix=0,zeropad=0,leftalign=0,space=0,possign=0;
      while (*fmt) {
             if (*fmt=='#') prefix=1;
        else if (*fmt=='0') zeropad=1;
        else if (*fmt=='-') leftalign=1;
        else if (*fmt==' ') space=1;
        else if (*fmt=='+') possign=1;
        else break;
        fmt++;
      }
      
      /* Width.
       */
      int width=0;
      if (*fmt=='*') {
        width=va_arg(vargs,int);
        fmt++;
      } else {
        while ((*fmt>='0')&&(*fmt<='9')) {
          width*=10;
          width+=(*fmt)-'0';
          if (width>100) width=100;
          fmt++;
        }
      }
      
      /* Precision.
       */
      int prec=0,preczero=0;
      if (*fmt=='.') {
        fmt++;
        if (*fmt=='*') {
          prec=va_arg(vargs,int);
          fmt++;
          if (prec<0) prec=0;
          else if (prec>100) prec=100;
        } else {
          while ((*fmt>='0')&&(*fmt<='9')) {
            prec*=10;
            prec+=(*fmt)-'0';
            if (prec>100) prec=100;
            fmt++;
          }
        }
        if (!prec) preczero=1;
      }
      
      /* Conversion specifier.
       */
      switch (*fmt) {
      
        case 'd':
        case 'i': {
            // TODO Width, Zero-Pad.
            int v=va_arg(vargs,int);
            int digitc=1;
            if (v<0) {
              int limit=-10;
              while (v<=limit) { digitc++; if (limit<INT_MIN/10) break; limit*=10; }
              if (digitc<prec) digitc=prec;
              if (dstc<dsta) dst[dstc]='-';
              dstc++;
              if (dstc<=dsta-digitc) {
                int i=digitc; for (;i-->0;v/=10) dst[dstc+i]='0'-(v%10);
              }
              dstc+=digitc;
            } else {
              int limit=10;
              while (v>=limit) { digitc++; if (limit>INT_MAX/10) break; limit*=10; }
              if (digitc<prec) digitc=prec;
              if (dstc<=dsta-digitc) {
                int i=digitc; for (;i-->0;v/=10) dst[dstc+i]='0'+(v%10);
              }
              dstc+=digitc;
            }
          } break;
          
        case 'o': {
            // TODO Width, Zero-Pad.
            unsigned int v=va_arg(vargs,unsigned int);
            int digitc=1;
            unsigned int limit=8;
            while (v>=limit) { digitc++; if (limit>UINT_MAX>>3) break; limit<<=3; }
            if (digitc<prec) digitc=prec;
            if (prefix) {
              if (dstc<=dsta-2) {
                dst[dstc]='0';
                dst[dstc+1]='o';
              }
              dstc+=2;
            }
            if (dstc<=dsta-digitc) {
              int i=digitc; for (;i-->0;v>>=3) dst[dstc+i]='0'+(v&7);
            }
            dstc+=digitc;
          } break;
          
        case 'u': {
            // TODO Width, Zero-Pad.
            unsigned int v=va_arg(vargs,unsigned int);
            int digitc=1;
            unsigned int limit=10;
            while (v>=limit) { digitc++; if (limit>UINT_MAX/10) break; limit*=10; }
            if (digitc<prec) digitc=prec;
            if (dstc<=dsta-digitc) {
              int i=digitc; for (;i-->0;v/=10) dst[dstc+i]='0'+(v%10);
            }
            dstc+=digitc;
          } break;
          
        case 'x':
        case 'X': {
            // TODO Width, Zero-Pad.
            unsigned int v=va_arg(vargs,unsigned int);
            int digitc=1;
            unsigned int limit=16;
            while (v>=limit) { digitc++; if (limit>UINT_MAX>>4) break; limit<<=4; }
            if (digitc<prec) digitc=prec;
            if (prefix) {
              if (dstc<=dsta-2) {
                dst[dstc]='0';
                dst[dstc+1]=*fmt;
              }
              dstc+=2;
            }
            if (dstc<=dsta-digitc) {
              const char *alphabet=(*fmt=='x')?"0123456789abcdef":"0123456789ABCDEF";
              int i=digitc; for (;i-->0;v>>=4) dst[dstc+i]=alphabet[v&15];
            }
            dstc+=digitc;
          } break;
          
        case 'e': case 'E':
        case 'f': case 'F':
        case 'g': case 'G':
        case 'a': case 'A': {
            // TODO Width, Zero-Pad.
            double v=va_arg(vargs,double);
            if (v<0.0) {
              if (dstc<dsta) dst[dstc]='-';
              dstc++;
              v=-v;
            }
            double whole,fract;
            fract=modf(v,&whole);
            
            int wholei=(whole>=1000000000.0)?999999999:(int)whole;
            int wdigitc=1,wlimit=10;
            while (wholei>wlimit) { wdigitc++; if (wlimit>INT_MAX/10) break; wlimit*=10; }
            if (dstc<=dsta-wdigitc) {
              int i=wdigitc; for (;i-->0;wholei/=10) dst[dstc+i]='0'+wholei%10;
            }
            dstc+=wdigitc;
            
            if (!preczero) {
              char fractv[9];
              int fractc=0;
              while (fractc<sizeof(fractv)) {
                fract*=10.0;
                int fi=(int)fract;
                fract-=fi;
                if (fi<0) fi=0; else if (fi>9) fi=9;
                fractv[fractc++]='0'+fi;
              }
              if (prec) {
                if (prec<fractc) fractc=prec;
              } else {
                int ninec=0;
                while ((ninec<fractc)&&(fractv[fractc-ninec-1]=='9')) ninec++;
                if ((ninec>=3)&&(ninec<fractc)) {
                  fractc-=ninec;
                  fractv[fractc-1]++;
                }
                while (fractc&&(fractv[fractc-1]=='0')) fractc--;
              }
              if (fractc) {
                if (dstc<dsta) dst[dstc]='.';
                dstc++;
                if (dstc<=dsta-fractc) memcpy(dst+dstc,fractv,fractc);
                dstc+=fractc;
              }
            }
          } break;
          
        case 'c': {
            // TODO Width.
            int v=va_arg(vargs,int);
            if (dstc<dsta) {
              if ((v>=0x20)&&(v<=0x7e)) dst[dstc]=v;
              else dst[dstc]='?';
            }
            dstc++;
          } break;
          
        case 'p': {
            // TODO Width, Zero-Pad.
            uintptr_t v=(uintptr_t)va_arg(vargs,void*);
            if (dstc<=dsta-2) {
              dst[dstc]='0';
              dst[dstc+1]='x';
            }
            dstc+=2;
            int digitc=sizeof(void*)<<1;
            if (dstc<=dsta-digitc) {
              int i=digitc; for (;i-->0;v>>=4) dst[dstc+i]="0123456789abcdef"[v&15];
            }
            dstc+=digitc;
          } break;
          
        case 's': {
            //TODO Width.
            const char *v=va_arg(vargs,char*);
            int c=0;
            if (v) {
              if (prec||preczero) {
                while ((c<prec)&&v[c]) c++;
              } else {
                while (v[c]) c++;
              }
            }
            if (dstc<dsta-c) memcpy(dst+dstc,v,c);
            dstc+=c;
          } break;
      
        default: {
            // Unknown specifier. Back it up to just after the '%', and emit a '%' verbatim.
            fmt=fmtstart;
            if (dstc<dsta) dst[dstc]='%';
            dstc++;
          }
      }
      fmt++;
    }
  }
  if (dstc<dsta) dst[dstc]=0;
  else if (dsta>0) dst[dsta-1]=0;
  return dstc;
}

/* fprintf
 */
 
int fprintf(void *unused,const char *fmt,...) {
  va_list vargs;
  va_start(vargs,fmt);
  char tmp[256];
  vsnprintf(tmp,sizeof(tmp),fmt,vargs);
  egg_log(tmp);
  return 0;
}

/* snprintf
 */
 
int snprintf(char *dst,int dsta,const char *fmt,...) {
  if (!dst||(dsta<0)) dsta=0;
  va_list vargs;
  va_start(vargs,fmt);
  return vsnprintf(dst,dsta,fmt,vargs);
}

/* fwrite. Annoyingly, clang replaces fprintf calls with this, if it sees there's no formatting.
 */
 
int fwrite(const void *src,int size,int count,void *file) {
  return fprintf(file,"%.*s",size*count,src);
}
