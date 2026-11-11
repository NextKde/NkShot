#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
import os
os.environ['UCX_VFS_ENABLE'] = 'n'
import json
from pathlib import Path
import sys
import ctranslate2
import sentencepiece
from ctranslate2.converters.marian import load_vocab
source, destination = Path(sys.argv[1]), Path(sys.argv[2])
sp = sentencepiece.SentencePieceProcessor(model_file=str(source / 'tiny-vocab.spm'))
vocab = destination / 'vocab.yml'
vocab.write_text(''.join(json.dumps(sp.id_to_piece(i), ensure_ascii=False) + ': ' + str(i) + '\n'
    for i in range(sp.get_piece_size())), encoding='utf-8')
if load_vocab(str(vocab)) != [sp.id_to_piece(i) for i in range(sp.get_piece_size())]:
    raise RuntimeError('Vocabulary identity check failed')
converter = ctranslate2.converters.MarianConverter(str(source / 'tiny-final.model.npz.best-perplexity.npz'), [str(vocab), str(vocab)])
converter.convert(str(destination / 'model'), quantization='int8')
