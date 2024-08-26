#ifndef MACIOC_INTERNAL_H
#define MACIOC_INTERNAL_H

#include "macioc.h"
#include <Cocoa/Cocoa.h>
#include "AKAppDelegate.h"
#include <stdio.h>

extern struct macioc {
  struct macioc_delegate delegate;
  int terminate;
} macioc;

#endif
