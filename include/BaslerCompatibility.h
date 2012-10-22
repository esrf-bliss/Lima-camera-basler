#ifndef BASLERCOMPATIBILITY_H
#define BASLERCOMPATIBILITY_H

#ifdef WIN32
#ifdef LIBBASLER_EXPORTS
#define LIBBASLER_API __declspec(dllexport)
#else
#define LIBBASLER_API __declspec(dllimport)
#endif
#else  /* Unix */
#define LIBBASLER_API
#endif

#endif
