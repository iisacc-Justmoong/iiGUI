
#ifndef IIGUI_PAINTER_EXPORT_H
#define IIGUI_PAINTER_EXPORT_H

#ifdef IIGUI_PAINTER_STATIC_DEFINE
#  define IIGUI_PAINTER_EXPORT
#  define IIGUI_PAINTER_NO_EXPORT
#else
#  ifndef IIGUI_PAINTER_EXPORT
#    ifdef iiGUIPainter_EXPORTS
        /* We are building this library */
#      define IIGUI_PAINTER_EXPORT 
#    else
        /* We are using this library */
#      define IIGUI_PAINTER_EXPORT 
#    endif
#  endif

#  ifndef IIGUI_PAINTER_NO_EXPORT
#    define IIGUI_PAINTER_NO_EXPORT 
#  endif
#endif

#ifndef IIGUI_PAINTER_DEPRECATED
#  define IIGUI_PAINTER_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef IIGUI_PAINTER_DEPRECATED_EXPORT
#  define IIGUI_PAINTER_DEPRECATED_EXPORT IIGUI_PAINTER_EXPORT IIGUI_PAINTER_DEPRECATED
#endif

#ifndef IIGUI_PAINTER_DEPRECATED_NO_EXPORT
#  define IIGUI_PAINTER_DEPRECATED_NO_EXPORT IIGUI_PAINTER_NO_EXPORT IIGUI_PAINTER_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef IIGUI_PAINTER_NO_DEPRECATED
#    define IIGUI_PAINTER_NO_DEPRECATED
#  endif
#endif

#endif /* IIGUI_PAINTER_EXPORT_H */
