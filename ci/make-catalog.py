#!/usr/bin/env python3
"""Build a private OpenCPN plugin catalog from the CI metadata files.

Usage: make-catalog.py <artifact-dir> <base-url> <out-dir>

Each platform's .xml metadata gets its tarball-url pointed at <base-url>,
and all of them are merged into <out-dir>/ocpn-plugins.xml next to copies
of the tarballs.
"""
import datetime, glob, os, re, shutil, sys

src, base, out = sys.argv[1:4]
os.makedirs(out, exist_ok=True)
entries = []
for xml in sorted(glob.glob(os.path.join(src, "**", "*.xml"), recursive=True)):
    text = open(xml, encoding="utf-8").read()
    m = re.search(r"<tarball-url>\s*(\S+)\s*</tarball-url>", text)
    if not m:
        continue
    name = m.group(1).rsplit("/", 1)[1]
    tarballs = glob.glob(os.path.join(os.path.dirname(xml), name))
    if not tarballs:
        sys.exit(f"tarball {name} missing next to {xml}")
    # Release asset names must stay plain: GitHub rewrites some characters.
    safe = re.sub(r"[^A-Za-z0-9._-]", "-", name)
    shutil.copy(tarballs[0], os.path.join(out, safe))
    text = text.replace(m.group(0), f"<tarball-url> {base}/{safe} </tarball-url>")
    text = re.sub(r"<\?xml[^>]*\?>", "", text)
    text = re.sub(r"<!--.*?-->", "", text, flags=re.S)
    entries.append(text.strip())
    print("added", name)
if not entries:
    sys.exit("no metadata found")
now = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M")
with open(os.path.join(out, "ocpn-plugins.xml"), "w", encoding="utf-8") as f:
    f.write('<?xml version="1.0" encoding="UTF-8"?>\n<plugins>\n')
    f.write(f"  <version>0.0.1</version>\n  <date>{now}</date>\n")
    f.write("\n".join(entries) + "\n</plugins>\n")
