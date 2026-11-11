#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Installer-only synthetic local check; never reads user text."""
import os
os.environ['UCX_VFS_ENABLE'] = 'n'
import sys
from pathlib import Path
import ctranslate2
import sentencepiece
root = Path(sys.argv[1])
for direction, text in [('en_zh', 'The file was saved.'), ('zh_en', '文件已经保存。')]:
    base = root / direction
    tokenizer = sentencepiece.SentencePieceProcessor(model_file=str(base / 'sentencepiece.model'))
    translator = ctranslate2.Translator(str(base / 'model'), device='cpu', compute_type='int8', intra_threads=2)
    result = translator.translate_batch([tokenizer.encode(text, out_type=str)], beam_size=4,
        length_penalty=0.2, replace_unknowns=True, max_input_length=0, max_decoding_length=512)[0]
    if not result.hypotheses[0] or len(result.hypotheses[0]) >= 512:
        raise RuntimeError('Local model self-check failed')
