#include "machid_internal.h"

/* Get string property.
 */

static char *machid_dev_get_string(struct machid_dev *dev,CFStringRef k) {
  char buf[256]={0};

  CFStringRef string=IOHIDDeviceGetProperty(dev->obj,k);
  if (!string) return 0;
  if (!CFStringGetCString(string,buf,sizeof(buf),kCFStringEncodingUTF8)) return 0;
  int bufc=0; while ((bufc<sizeof(buf))&&buf[bufc]) bufc++;
  if (!bufc) return 0;

  /* Force non-G0 to space, then trim leading and trailing spaces. */
  int i; for (i=0;i<bufc;i++) {
    if ((buf[i]<0x20)||(buf[i]>0x7e)) buf[i]=0x20;
  }
  while (bufc&&(buf[bufc-1]==0x20)) bufc--;
  int leadc=0; while ((leadc<bufc)&&(buf[leadc]==0x20)) leadc++;
  if (leadc==bufc) return 0;
  
  char *dst=malloc(bufc-leadc+1);
  if (!dst) return 0;
  memcpy(dst,buf+leadc,bufc-leadc);
  dst[bufc-leadc]=0;
  return dst;
}

/* Make the device name.
 * Normally this is MFR + " " + PROD.
 * Drop MFR if it's a prefix of PROD, I wouldn't be surprised if that happens. (but no evidence of it just now).
 */

static void machid_dev_compose_name(struct machid_dev *dev,const char *a,const char *b) {
  int ac=0; if (a) while (a[ac]) ac++;
  int bc=0; if (b) while (b[bc]) bc++;

  if ((ac<=bc)&&!memcmp(a,b,ac)) ac=0;

  if (dev->name) {
    free(dev->name);
    dev->name=0;
  }

  if (ac&&bc) {
    int dstc=ac+1+bc;
    char *dst=malloc(dstc+1);
    if (!dst) return;
    memcpy(dst,a,ac);
    dst[ac]=' ';
    memcpy(dst+ac+1,b,bc);
    dst[dstc]=0;
    dev->name=dst;
    return;
  }

  if (!ac) {
    a=b;
    ac=bc;
  }
  if (!ac) return;
  char *dst=malloc(ac+1);
  if (!dst) return;
  memcpy(dst,a,ac);
  dst[ac]=0;
  dev->name=dst;
}

/* Apply input element.
 */

static void machid_dev_apply_IOHIDElement(struct machid_dev *dev,IOHIDElementRef element) {
      
  IOHIDElementCookie cookie=IOHIDElementGetCookie(element);
  if (cookie>INT_MAX) return;
  CFIndex lo=IOHIDElementGetLogicalMin(element);
  CFIndex hi=IOHIDElementGetLogicalMax(element);
  if (lo>INT_MAX) lo=INT_MAX;
  if (hi>INT_MAX) hi=INT_MAX;
  if (lo>=hi) return;
  uint16_t usagepage=IOHIDElementGetUsagePage(element);
  uint16_t usage=IOHIDElementGetUsage(element);

  IOHIDValueRef value=0;
  int v=0;
  if (IOHIDDeviceGetValue(dev->obj,element,&value)==kIOReturnSuccess) {
    v=IOHIDValueGetIntegerValue(value);
  }

  struct machid_btn *btn=machid_dev_btnv_add(dev,cookie);
  if (!btn) return;
  btn->usage=(usagepage<<16)|usage;
  btn->value=v;
  btn->lo=lo;
  btn->hi=hi;
}

/* Setup.
 */
 
void machid_dev_setup(struct machid *machid,struct machid_dev *dev) {
  
  char *mfrname=machid_dev_get_string(dev,CFSTR(kIOHIDManufacturerKey));
  char *prodname=machid_dev_get_string(dev,CFSTR(kIOHIDProductKey));
  machid_dev_compose_name(dev,mfrname,prodname);
  if (mfrname) free(mfrname);
  if (prodname) free(prodname);

  CFArrayRef elements=IOHIDDeviceCopyMatchingElements(dev->obj,0,0);
  if (elements) {
    CFTypeID elemtypeid=IOHIDElementGetTypeID();
    CFIndex elemc=CFArrayGetCount(elements);
    CFIndex elemi; for (elemi=0;elemi<elemc;elemi++) {
      IOHIDElementRef element=(IOHIDElementRef)CFArrayGetValueAtIndex(elements,elemi);
      if (element&&(CFGetTypeID(element)==elemtypeid)) {
        machid_dev_apply_IOHIDElement(dev,element);
      }
    }
    CFRelease(elements);
  }
}

/* Button list.
 */
 
int machid_dev_btnv_search(const struct machid_dev *dev,int btnid) {
  int lo=0,hi=dev->btnc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct machid_btn *btn=dev->btnv+ck;
         if (btnid<btn->btnid) hi=ck;
    else if (btnid>btn->btnid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

struct machid_btn *machid_dev_btnv_add(struct machid_dev *dev,int btnid) {
  int p=machid_dev_btnv_search(dev,btnid);
  if (p>=0) return 0;
  p=-p-1;

  if (dev->btnc>=dev->btna) {
    int na=dev->btna+16;
    if (na>INT_MAX/sizeof(struct machid_btn)) return 0;
    void *nv=realloc(dev->btnv,sizeof(struct machid_btn)*na);
    if (!nv) return 0;
    dev->btnv=nv;
    dev->btna=na;
  }

  struct machid_btn *btn=dev->btnv+p;
  memmove(btn+1,btn,sizeof(struct machid_btn)*(dev->btnc-p));
  dev->btnc++;
  memset(btn,0,sizeof(struct machid_btn));
  btn->btnid=btnid;
  return btn;
}

struct machid_btn *machid_dev_btnv_get(const struct machid_dev *dev,int btnid) {
  int p=machid_dev_btnv_search(dev,btnid);
  if (p<0) return 0;
  return dev->btnv+p;
}
