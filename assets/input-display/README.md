# InputDisplay artwork

Original, unmodified `btn/neogeo` and `dir/stick/1..9` PNGs from
https://github.com/Delta3-Studio/InputDisplay at commit
`3a60d387a07f5d472ed6934f1f8c0508fea71db6`.

Copyright belongs to the InputDisplay authors/contributors (Delta3-Studio;
the project README also credits its origin as lucasteles/InputDisplay).
Distributed under the repository's GPL-3.0 licence, included in LICENSE.
No separate asset notices were found in the inspected tree. Source blob/SHA256
verification is recorded in provenance.json. No application executable imported.

`tools/embed_input_art.py` decodes the PNGs to exact RGBA and zlib-compresses them
into `port/input_art_data.h`. It does not crop, repaint or modify the source art.
These files document the initial sprite-based prototype. After player review,
the active renderer draws original pixel geometry/letters directly at native
resolution instead; it no longer includes the generated sprites or decodes them.
The original PNGs, pinned provenance and licence are retained as historical
reference. Regenerating this header does not enable it in the active renderer.
Regeneration uses Pillow as a development tool; the application needs only its
existing zlib dependency, and no external PNGs/application at runtime.
