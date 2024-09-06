/* sfgc.h
 * Sound effects compiler.
 * This is a pretty dumb compiler; the text and binary formats are structured very similar.
 */
 
#ifndef SFGC_H
#define SFGC_H

struct sr_encoder;

int sfg_compile(struct sr_encoder *dst,const char *src,int srcc,const char *refname);

int sfg_uncompile(struct sr_encoder *dst,const void *src,int srcc,const char *refname);

#endif
