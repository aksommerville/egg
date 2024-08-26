/* hostio_input.h
 */
 
#ifndef HOSTIO_INPUT_H
#define HOSTIO_INPUT_H

struct hostio_input;
struct hostio_input_type;
struct hostio_input_delegate;
struct hostio_input_setup;

struct hostio_input_delegate {
  void *userdata;
  void (*cb_connect)(struct hostio_input *driver,int devid);
  void (*cb_disconnect)(struct hostio_input *driver,int devid);
  void (*cb_button)(struct hostio_input *driver,int devid,int btnid,int value);
};

struct hostio_input {
  const struct hostio_input_type *type;
  struct hostio_input_delegate delegate;
};

struct hostio_input_setup {
  const char *path;
};

struct hostio_input_type {
  const char *name;
  const char *desc;
  int objlen;
  int appointment_only;
  void (*del)(struct hostio_input *driver);
  int (*init)(struct hostio_input *driver,const struct hostio_input_setup *setup);
  
  /* Driver should only fire callbacks during update.
   * Don't call cb_connect during init, wait for the first update.
   * Don't call cb_disconnect during del or on manual disconnects.
   */
  int (*update)(struct hostio_input *driver);
  
  int (*devid_by_index)(const struct hostio_input *driver,int p);
  void (*disconnect)(struct hostio_input *driver,int devid);
  const char *(*get_ids)(int *vid,int *pid,int *version,struct hostio_input *driver,int devid);
  int (*for_each_button)(
    struct hostio_input *driver,
    int devid,
    int (*cb)(int btnid,int hidusage,int lo,int hi,int value,void *userdata),
    void *userdata
  );
};

void hostio_input_del(struct hostio_input *driver);

struct hostio_input *hostio_input_new(
  const struct hostio_input_type *type,
  const struct hostio_input_delegate *delegate,
  const struct hostio_input_setup *setup
);

const struct hostio_input_type *hostio_input_type_by_index(int p);
const struct hostio_input_type *hostio_input_type_by_name(const char *name,int namec);

int hostio_input_devid_next();

#endif
