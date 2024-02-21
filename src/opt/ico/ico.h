/* ico.h
 * Helper for reading Microsoft icon files.
 * Which happens to also be the "favicon" format.
 */
 
#ifndef ICO_H
#define ICO_H

struct ico {
  struct ico_image {
    const void *serial; // WEAK, points into the original source.
    int serialc;
    int w,h;
    int ctc,planec,pixelsize; // from header
    int depth,colortype; // if PNG
    int stride;
    void *v;
  } *imagev;
  int imagec,imagea;
};

void ico_del(struct ico *ico);

/* Provide a complete ".ico" file.
 * Caller must keep that serial data in scope as long as the ico object lives.
 */
struct ico *ico_new(const void *src,int srcc);

/* Find the most approriate image, decode it, and return a WEAK reference.
 * (obo) means return the closest match to (w,h), by some opaque algorithm. Otherwise exact matches only.
 * (force_rgba) means the returned image will be RGBA with minimum stride.
 */
struct ico_image *ico_get_image(struct ico *ico,int w,int h,int obo,int force_rgba);

/* Decode and reformat one image in place as needed.
 * Returns the same image on success.
 * Use this instead of ico_get_image() if you want to pick one manually from (ico->imagev).
 */
struct ico_image *ico_ready_image(
  struct ico *ico,
  struct ico_image *image,
  int force_rgba
);

#endif
