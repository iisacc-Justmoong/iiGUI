
#ifndef IIGUI_EXPORT_H
#define IIGUI_EXPORT_H

#ifdef IIGUI_STATIC_DEFINE
#  define IIGUI_EXPORT
#  define IIGUI_NO_EXPORT
#else
#  ifndef IIGUI_EXPORT
#    ifdef iiGUI_EXPORTS
        /* We are building this library */
#      define IIGUI_EXPORT 
#    else
        /* We are using this library */
#      define IIGUI_EXPORT 
#    endif
#  endif

#  ifndef IIGUI_NO_EXPORT
#    define IIGUI_NO_EXPORT 
#  endif
#endif

#ifndef IIGUI_DEPRECATED
#  define IIGUI_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef IIGUI_DEPRECATED_EXPORT
#  define IIGUI_DEPRECATED_EXPORT IIGUI_EXPORT IIGUI_DEPRECATED
#endif

#ifndef IIGUI_DEPRECATED_NO_EXPORT
#  define IIGUI_DEPRECATED_NO_EXPORT IIGUI_NO_EXPORT IIGUI_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef IIGUI_NO_DEPRECATED
#    define IIGUI_NO_DEPRECATED
#  endif
#endif

#endif /* IIGUI_EXPORT_H */
