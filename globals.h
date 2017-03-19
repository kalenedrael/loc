#ifndef _GLOBALS_H_
#define _GLOBALS_H_

#ifdef USE_DOUBLE
typedef double real_t;
#define glTranslate(...) glTranslated(__VA_ARGS__)
#define glScale(...) glScaled(__VA_ARGS__)
#define glRotate(...) glRotated(__VA_ARGS__)
#define glColor4(...) glColor4d(__VA_ARGS__)
#define GL_SIZE GL_DOUBLE
#else
typedef float real_t;
#define glTranslate(...) glTranslatef(__VA_ARGS__)
#define glScale(...) glScalef(__VA_ARGS__)
#define glRotate(...) glRotatef(__VA_ARGS__)
#define glColor4(...) glColor4f(__VA_ARGS__)
#define sqrt(x) sqrtf(x)
#define sin(x) sinf(x)
#define floor(x) floorf(x)
#define round(x) roundf(x)
#define GL_SIZE GL_FLOAT
#endif /* USE_DOUBLE */

#define XCOR_LEN 512 /* samples */
#define SND_SPEED 343.0 /* m/s */
#define XRES 1200
#define YRES 1200

#endif /* _GLOBALS_H_ */
