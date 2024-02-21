/* hostio_input.h
 * Provides event-driven input devices, usually joysticks.
 */
 
#ifndef HOSTIO_INPUT_H
#define HOSTIO_INPUT_H

struct hostio_input;
struct hostio_input_type;
struct hostio_input_delegate;
struct hostio_input_setup;

struct hostio_input_delegate {
  void *userdata;
  void (*cb_connect)(void *userdata,int devid);
  void (*cb_disconnect)(void *userdata,int devid);
  void (*cb_button)(void *userdata,int devid,int btnid,int value);
};

struct hostio_input_setup {
  int dummy;
};

struct hostio_input {
  const struct hostio_input_type *type;
  struct hostio_input_delegate delegate;
};

struct hostio_input_type {
  const char *name;
  const char *desc;
  int objlen;
  int appointment_only;
  void (*del)(struct hostio_input *driver);
  int (*init)(struct hostio_input *driver,const struct hostio_input_setup *setup);
  int (*update)(struct hostio_input *driver);
  int (*devid_by_index)(const struct hostio_input *driver,int p);
  const char *(*get_ids)(int *vid,int *pid,int *version,struct hostio_input *driver,int devid);
  int (*list_buttons)(
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

const struct hostio_input_type *hostio_input_type_by_name(const char *name,int namec);
const struct hostio_input_type *hostio_input_type_by_index(int p);

int hostio_input_devid_next();

#endif
