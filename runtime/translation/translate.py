#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""One local request on stdin, one result on stdout. No installation or networking."""
import json
import os
from pathlib import Path
import re
import sys

os.environ['UCX_VFS_ENABLE'] = 'n'  # Disable optional UCX filesystem diagnostics.

def offline_audit(event, args):
    if event.startswith('socket.'):
        raise PermissionError('network disabled')
sys.addaudithook(offline_audit)

class Failure(Exception):
    def __init__(self, code):
        self.code = code

def run():
    try:
        raw = sys.stdin.buffer.read(65537)
        if len(raw) > 65536:
            raise Failure('input_too_long')
        request = json.loads(raw.decode('utf-8'))
        if not isinstance(request, dict):
            raise Failure('input_invalid')
        direction, text = request.get('direction'), request.get('text')
        if direction not in ('en_zh', 'zh_en') or not isinstance(text, str) or not text.strip():
            raise Failure('input_invalid')
        if len(text) > 4000 or len(text.encode('utf-8')) > 16000:
            raise Failure('input_too_long')
    except (UnicodeError, ValueError, TypeError):
        raise Failure('input_invalid') from None
    root_name = os.environ.get('NKSHOT_TRANSLATION_MODEL_DIR', '')
    root = Path(root_name)
    if not root_name or not root.is_absolute() or not root.is_dir():
        raise Failure('model_missing')
    if (root.parent / 'INSTALLATION-INCOMPLETE').exists():
        raise Failure('model_invalid')
    base = root / direction
    model, tokenizer = base / 'model', base / 'sentencepiece.model'
    if not model.is_dir() or not (model / 'model.bin').is_file() or not tokenizer.is_file():
        raise Failure('model_missing')
    try:
        root = root.resolve(strict=True)
        if not model.resolve(strict=True).is_relative_to(root) or not tokenizer.resolve(strict=True).is_relative_to(root):
            raise Failure('model_invalid')
        metadata = json.loads((base / 'metadata.json').read_text(encoding='utf-8'))
        if not isinstance(metadata, dict) or metadata.get('direction') != direction or metadata.get('runtimeSchema') != 1:
            raise Failure('model_invalid')
    except (OSError, ValueError, TypeError):
        raise Failure('model_invalid') from None
    try:
        import ctranslate2
        import sentencepiece
    except Exception:
        raise Failure('runtime_unavailable') from None
    try:
        processor = sentencepiece.SentencePieceProcessor(model_file=str(tokenizer))
        translator = ctranslate2.Translator(str(model), device='cpu', compute_type='int8', inter_threads=1, intra_threads=2)
    except Exception:
        raise Failure('model_invalid') from None
    paragraphs = []
    for paragraph in text.split('\n'):
        if not paragraph.strip():
            paragraphs.append('')
            continue
        sentences = re.split(r'(?<=[。！？!?])\s*|(?<=\.)\s+', paragraph.strip())
        translated = []
        for sentence in sentences:
            if not sentence:
                continue
            tokens = processor.encode(sentence, out_type=str)
            if len(tokens) > 384:
                raise Failure('sentence_too_long')
            result = translator.translate_batch([tokens], beam_size=4, length_penalty=0.2,
                replace_unknowns=True, max_input_length=0, max_decoding_length=512)[0]
            pieces = result.hypotheses[0]
            if len(pieces) >= 512:
                raise Failure('output_too_long')
            # Argos SentencePieceTokenizer target-piece decoding, not source SP IDs.
            translated.append(''.join(pieces).replace('▁', ' ').strip())
        paragraphs.append(('' if direction == 'en_zh' else ' ').join(translated))
    output = '\n'.join(paragraphs)
    if not output.strip():
        raise Failure('translation_failed')
    response = {'ok': True, 'text': output}
    if len(json.dumps(response, ensure_ascii=False).encode('utf-8')) > 60000:
        raise Failure('output_too_long')
    return response

def main():
    # Native runtime diagnostics must not corrupt JSON or log recognized text.
    stdout_fd, stderr_fd = os.dup(1), os.dup(2)
    with open(os.devnull, 'wb') as null:
        os.dup2(null.fileno(), 1)
        os.dup2(null.fileno(), 2)
        try:
            result = run()
        except Failure as error:
            result = {'ok': False, 'errorCode': error.code}
        except Exception:
            result = {'ok': False, 'errorCode': 'translation_failed'}
        finally:
            sys.stdout.flush()
            sys.stderr.flush()
            os.dup2(stdout_fd, 1)
            os.dup2(stderr_fd, 2)
            os.close(stdout_fd)
            os.close(stderr_fd)
    print(json.dumps(result, ensure_ascii=False), flush=True)

if __name__ == '__main__':
    main()
