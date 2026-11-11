# Local screenshot translation

NkShot translates recognized text locally between English and Chinese. It does not upload the image or OCR text, does not use a cloud fallback, and never installs software while translating. Always check the recognized text first: OCR mistakes, names, and technical terminology can be mistranslated. Keep the original alongside the machine translation; do not rely on it for consequential decisions.

## Install once

Requires Linux x86_64, Python 3.12 or 3.14 with `venv` support, and internet access **for this explicit installation only**. On Debian/Ubuntu, install the matching `python3-venv` package if necessary. The installer downloads about 139 MB of model sources plus roughly 62 MB of runtime wheels; allow about 700 MB of free space for downloaded sources, virtual environment, and converted models. No Torch or Stanza runtime is installed.

From the NkShot source checkout:

```sh
python3 runtime/translation/install.py
```

The destination defaults to `$XDG_DATA_HOME/nkshot/translation`, or `~/.local/share/nkshot/translation` if XDG_DATA_HOME is unset. NkShot discovers this installation automatically. Restart NkShot, recognize text from an image or selected region, choose English → Chinese or Chinese → English, and select Translate. Copy translation only when you want to change the clipboard.

An existing destination is never overwritten. A failed new installation leaves an `INSTALLATION-INCOMPLETE` marker and preserves its directory for inspection; choose a different destination or inspect and remove only that failed installation yourself. No automatic upgrades or deletion occur.

For a custom destination:

```sh
python3 runtime/translation/install.py --destination "$HOME/nkshot-translation"
"$HOME/nkshot-translation/run-nkshot"
```

The generated launcher supplies the correct local paths and starts `nkshot`; it requires NkShot on PATH. A verified local download cache may be supplied with `--cache /absolute/cache`; files are accepted only when their byte counts and SHA256 match the manifest. The cache is not modified.

## Limits and cancellation

Each operation supports at most 4,000 characters (the application uses the stricter UTF-16 count) and 16,000 UTF-8 bytes. Paragraph breaks are retained. A sentence above 384 model tokens is rejected rather than silently truncated; select a shorter passage. An output reaching 512 tokens per sentence is rejected. Sentence splitting is conservative punctuation-based; abbreviations and complex layouts can reduce quality.

The application runs a separate local worker, closes its stdin after one request, and controls cancellation/timeouts. No text is passed in command-line arguments. The worker emits a single JSON result, never raw exceptions, and suppresses native runtime diagnostics. It uses a Python audit hook to reject Python socket operations; this is not an OS-level network sandbox claim. Its inference path contains no download or remote inference code.

## Runtime contract and diagnostics

Default installation contents:

- `venv/bin/python`: isolated Python runtime with pinned packages.
- `translate.py`: one-shot local JSON worker.
- `models/en_zh/{metadata.json,sentencepiece.model,model/}`.
- `models/zh_en/{metadata.json,sentencepiece.model,model/}`.

Advanced/testing overrides are three explicit absolute paths: `NKSHOT_TRANSLATION_PYTHON`, `NKSHOT_TRANSLATION_RUNNER`, `NKSHOT_TRANSLATION_MODEL_DIR`. These are not normally needed.

Worker invocation: `python -I -u translate.py`; stdin is one JSON object `{"direction":"en_zh","text":"…"}` or direction `zh_en`, followed by EOF. Stdout is one object `{"ok":true,"text":"…"}` or `{"ok":false,"errorCode":"…"}`.

Stable error codes: `input_invalid`, `input_too_long`, `sentence_too_long`, `output_too_long`, `model_missing`, `model_invalid`, `runtime_unavailable`, `translation_failed`.

## Models, licenses, and quality boundary

- English → Chinese: Argos `translate-en_zh-1_9`, derived from OPUS-MT. The actual package README declares the original model **CC-BY 4.0**, credited to Jörg Tiedemann and Santhosh Thottingal, “OPUS-MT — Building open translation services for the World”, EAMT 2020. Preserve the original README and notices. No weights are changed by this installer.
- Chinese → English: official Helsinki-NLP `opus-mt_tiny_zho-eng`, **Apache-2.0** according to its model card. The installer converts Marian weights to CTranslate2 int8. All 32,000 SentencePiece vocabulary IDs are preserved and checked. Model card, source hashes, and modification notice are retained.
- Dependencies: CTranslate2 4.9.0, SentencePiece 0.2.3, NumPy 2.5.4, PyYAML 6.0.3. Their licenses remain in the installed distributions.

The older Argos Chinese → English model produced severe errors in ordinary screenshot instructions during evaluation and is deliberately not installed. Fixed normal-language samples support these selected directions as a translation aid, not a guarantee. Known English → Chinese limitations include “undo” becoming “cancel”, “recording” becoming audio recording, and awkward shipping terminology. Garbled OCR can produce confidently wrong translations. No hardcoded translations or dictionary substitutions hide these limitations.

Model binaries and virtual environments are local installation artifacts, not files to commit to Git. Exact official URLs, byte counts, and SHA256 are in `runtime/translation/manifest.json`. Installation fails closed on a changed download; do not disable those checks.
