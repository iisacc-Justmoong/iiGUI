"""Check the Android editor layout JNI calls against the installed SDK."""
from pathlib import Path
import argparse
import subprocess
from android_text_signatures import declaration_matches

METHODS = {
    "android.graphics.Paint": (("setTextSize", "(F)V"), ("setColor", "(I)V"),
        ("setTypeface", "(Landroid/graphics/Typeface;)Landroid/graphics/Typeface;"),
        ("setAntiAlias", "(Z)V"), ("setSubpixelText", "(Z)V")),
    "android.text.TextPaint": (("<init>", "()V"),),
    "android.text.SpannableString": (("<init>", "(Ljava/lang/CharSequence;)V"),
        ("setSpan", "(Ljava/lang/Object;III)V")),
    "android.text.style.TabStopSpan$Standard": (("<init>", "(I)V"),),
    "android.text.StaticLayout$Builder": (
        ("obtain", "(Ljava/lang/CharSequence;IILandroid/text/TextPaint;I)Landroid/text/StaticLayout$Builder;"),
        ("setMaxLines", "(I)Landroid/text/StaticLayout$Builder;"),
        ("setEllipsize", "(Landroid/text/TextUtils$TruncateAt;)Landroid/text/StaticLayout$Builder;"),
        ("setEllipsizedWidth", "(I)Landroid/text/StaticLayout$Builder;"),
        ("setUseLineSpacingFromFallbacks", "(Z)Landroid/text/StaticLayout$Builder;"),
        ("build", "()Landroid/text/StaticLayout;")),
    "android.text.Layout": (("getDesiredWidth", "(Ljava/lang/CharSequence;Landroid/text/TextPaint;)F"),
        ("getLineCount", "()I"), ("getLineStart", "(I)I"), ("getLineEnd", "(I)I"),
        ("getEllipsisCount", "(I)I"),
        ("getLineTop", "(I)I"), ("getLineBottom", "(I)I"), ("getLineBaseline", "(I)I"),
        ("getLineLeft", "(I)F"), ("getLineRight", "(I)F"), ("getParagraphDirection", "(I)I"), ("getLineWidth", "(I)F"),
        ("getPrimaryHorizontal", "(I)F"), ("getOffsetForHorizontal", "(IF)I"),
        ("isRtlCharAt", "(I)Z"), ("getSelectionPath", "(IILandroid/graphics/Path;)V"),
        ("draw", "(Landroid/graphics/Canvas;)V")),
    "android.graphics.Path": (("<init>", "()V"), ("transform", "(Landroid/graphics/Matrix;)V")),
    "android.graphics.Matrix": (("<init>", "()V"), ("setScale", "(FF)V")),
    "android.graphics.Region": (("<init>", "()V"), ("<init>", "(IIII)V"),
        ("setPath", "(Landroid/graphics/Path;Landroid/graphics/Region;)Z")),
    "android.graphics.RegionIterator": (("<init>", "(Landroid/graphics/Region;)V"),
        ("next", "(Landroid/graphics/Rect;)Z")),
    "android.graphics.Rect": (("<init>", "()V"),),
    "android.graphics.Bitmap": (("createBitmap", "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;"),
        ("eraseColor", "(I)V")),
    "android.graphics.Canvas": (("<init>", "(Landroid/graphics/Bitmap;)V"),
        ("scale", "(FF)V"), ("translate", "(FF)V"), ("clipRect", "(FFFF)Z")),
}
FIELDS = {
    "android.graphics.Rect": (("left", "I"), ("right", "I")),
    "android.text.Spanned": (("SPAN_EXCLUSIVE_EXCLUSIVE", "I"),),
    "android.graphics.Bitmap$Config": (("ARGB_8888", "Landroid/graphics/Bitmap$Config;"),),
    "android.text.TextUtils$TruncateAt": (("END", "Landroid/text/TextUtils$TruncateAt;"),),
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--jar", type=Path, required=True)
    parser.add_argument("--javap", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    source = "\n".join((root / name).read_text() for name in
                       ("src/Text/TextLineAndroid.cpp", "src/Text/AndroidTextSupport.h"))
    for owner in METHODS.keys() | FIELDS.keys():
        output = subprocess.run([str(args.javap), "-s", "-classpath", str(args.jar), owner],
                                check=True, text=True, capture_output=True).stdout
        for field, entries in ((False, METHODS.get(owner, ())), (True, FIELDS.get(owner, ()))):
            for name, descriptor in entries:
                if not declaration_matches(output, name, descriptor, owner, field):
                    raise RuntimeError(f"Android SDK does not provide {owner}.{name} {descriptor}")
                if f'"{name}", "{descriptor}"' not in source:
                    raise RuntimeError(f"Android editor source does not use {owner}.{name} {descriptor}")
    print("Android editor JNI signatures match the installed SDK")


if __name__ == "__main__":
    main()
