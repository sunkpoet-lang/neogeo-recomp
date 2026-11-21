# Recorder input artwork

Generated with the user's existing recorder `Pintor._boton` and `Pintor._bola`
methods, not a reinterpreted shader. Original recorder palette and Barlow
Condensed font rendering retained. Source/font hashes and output hashes are in
provenance.json. Used with the user's explicit request to reuse this design.
The runtime needs neither Python, Pillow nor the recorder. The font is rendered
into these images; no font binary is bundled. The stick gate uses the recorder's
original circles/colours and nine ball positions. Only current controls are used,
without player profiles, history, flags, replay logs or the recording service.

The supplied PNGs are sufficient to regenerate the embedded runtime header:
`python tools/embed_recorder_input_art.py` (Pillow required for this developer
step). `python tools/embed_recorder_input_art.py --check` verifies provenance
and header bytes. Neither command needs the original recorder or font binary.
