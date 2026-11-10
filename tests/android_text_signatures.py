"""Check native text JNI descriptors against the installed Android SDK API."""
from pathlib import Path
import argparse
import re
import subprocess


METHODS = {
    "android.graphics.Paint": (
        ("setTextSize", "(F)V"), ("setColor", "(I)V"),
        ("setTypeface", "(Landroid/graphics/Typeface;)Landroid/graphics/Typeface;"),
        ("setAntiAlias", "(Z)V"), ("setSubpixelText", "(Z)V"), ("setUnderlineText", "(Z)V"),
    ),
    "android.text.TextPaint": (("<init>", "()V"),),
    "android.text.StaticLayout$Builder": (
        ("obtain", "(Ljava/lang/CharSequence;IILandroid/text/TextPaint;I)Landroid/text/StaticLayout$Builder;"),
        ("setUseLineSpacingFromFallbacks", "(Z)Landroid/text/StaticLayout$Builder;"),
        ("setEllipsize", "(Landroid/text/TextUtils$TruncateAt;)Landroid/text/StaticLayout$Builder;"),
        ("setMaxLines", "(I)Landroid/text/StaticLayout$Builder;"),
        ("build", "()Landroid/text/StaticLayout;"),
    ),
    "android.text.Layout": (
        ("getLineCount", "()I"), ("getLineLeft", "(I)F"), ("getLineRight", "(I)F"),
        ("getParagraphDirection", "(I)I"), ("getLineWidth", "(I)F"),
        ("getLineTop", "(I)I"), ("getLineBottom", "(I)I"), ("getHeight", "()I"),
        ("draw", "(Landroid/graphics/Canvas;)V"),
    ),
    "android.graphics.Bitmap": (
        ("createBitmap", "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;"),
        ("eraseColor", "(I)V"),
    ),
    "android.graphics.Canvas": (
        ("<init>", "(Landroid/graphics/Bitmap;)V"), ("save", "()I"),
        ("restoreToCount", "(I)V"), ("clipRect", "(FFFF)Z"),
        ("translate", "(FF)V"), ("scale", "(FF)V"),
    ),
    "android.graphics.Typeface": (
        ("createFromAsset", "(Landroid/content/res/AssetManager;Ljava/lang/String;)Landroid/graphics/Typeface;"),
        ("createFromFile", "(Ljava/lang/String;)Landroid/graphics/Typeface;"),
    ),
    "android.graphics.Typeface$Builder": (
        ("<init>", "(Landroid/content/res/AssetManager;Ljava/lang/String;)V"),
        ("<init>", "(Ljava/lang/String;)V"), ("setTtcIndex", "(I)Landroid/graphics/Typeface$Builder;"),
        ("build", "()Landroid/graphics/Typeface;"),
    ),
    "android.content.Context": (("getAssets", "()Landroid/content/res/AssetManager;"),),
}
FIELDS = {
    "android.graphics.Bitmap$Config": (("ARGB_8888", "Landroid/graphics/Bitmap$Config;"),),
    "android.text.TextUtils$TruncateAt": (("END", "Landroid/text/TextUtils$TruncateAt;"),),
    "android.os.Build$VERSION": (("SDK_INT", "I"),),
}


def declaration_matches(output, name, descriptor, owner, field=False):
    lines = output.splitlines()
    constructor = owner.rsplit(".", 1)[-1]
    for i, line in enumerate(lines[:-1]):
        token = constructor if name == "<init>" else name
        pattern = r"\b" + re.escape(token) + (r"\s*;" if field else r"\s*\(")
        if re.search(pattern, line) and lines[i + 1].strip() == "descriptor: " + descriptor:
            return True
    return False


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--jar", type=Path, required=True)
    parser.add_argument("--javap", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    source = "\n".join((root / name).read_text() for name in
                       ("src/Text/TextAndroid.cpp", "src/Text/AndroidTextSupport.h"))
    for owner in METHODS.keys() | FIELDS.keys():
        result = subprocess.run([str(args.javap), "-s", "-classpath", str(args.jar), owner],
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError(f"Cannot inspect Android API {owner}:\n{result.stdout}")
        for field, entries in ((False, METHODS.get(owner, ())), (True, FIELDS.get(owner, ()))):
            for name, descriptor in entries:
                if not declaration_matches(result.stdout, name, descriptor, owner, field):
                    raise RuntimeError(f"Android SDK does not provide {owner}.{name} {descriptor}")
                if f'"{name}", "{descriptor}"' not in source:
                    raise RuntimeError(f"Android text source does not use {owner}.{name} {descriptor}")
    print("Android text JNI methods and fields match the installed Android SDK")


if __name__ == "__main__":
    main()
