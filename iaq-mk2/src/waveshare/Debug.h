// IAQ-MK2 replacement for Waveshare's Debug.h: the vendor version prints
// "e-Paper busy" lines inside polling loops and pulls in Wire.h. Logging is
// done by the application, so the vendor macro is a no-op here.
#ifndef __DEBUG_H
#define __DEBUG_H
#define Debug(__info) ((void)0)
#endif
