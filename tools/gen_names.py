#!/usr/bin/env python3
"""Baja de PokeAPI los nombres oficiales por idioma y escribe tools/dex_names.py.

  python3 tools/gen_names.py

Solo FR y DE tienen nombres propios en gen 1; ES/IT/PT usan los ingleses, asi
que aqui se guarda unicamente lo que difiere del ingles. La fuente GFX es
ASCII, asi que los nombres salen en mayusculas y sin acentos.
"""
import json
import os
import subprocess
import time
import unicodedata

LANGS = ('fr', 'de')
LANGS_UTF8 = ('ja-hrkt',)  # CJK: van tal cual en UTF-8, no en octal ASCII


def sin_genero(s):
    """Nidoran hembra/macho: ♀ y ♂ no existen en ninguna fuente que usemos.

    Ni la tabla CP437 de glcdfont.h los tiene en una posicion imprimible, ni
    los trae ningun subconjunto unifont de U8g2 (japanese*, korean*, chinese*).
    Y Arduino_GFX no dibuja nada cuando le falta el glifo, ni siquiera avanza
    el cursor, asi que sin esto los dos Nidoran salen con el mismo nombre.
    """
    return s.replace('♀', 'F').replace('♂', 'M')


def ascii_up(s):
    """'Salamèche' -> 'SALAMECHE'; conserva los simbolos de genero como f/m."""
    s = sin_genero(s)
    s = unicodedata.normalize('NFD', s)
    s = ''.join(c for c in s if unicodedata.category(c) != 'Mn')
    return s.upper()


def fetch(num):
    url = f'https://pokeapi.co/api/v2/pokemon-species/{num}/'
    for intento in range(4):
        r = subprocess.run(['curl', '-s', '--max-time', '25', '-A', 'Mozilla/5.0', url],
                           capture_output=True, text=True)
        if r.returncode == 0 and r.stdout.strip().startswith('{'):
            return json.loads(r.stdout)
        time.sleep(1 + intento)
    raise SystemExit(f'no se pudo bajar la especie {num}')


def main():
    out = {}
    for num in range(1, 152):
        d = fetch(num)
        names = {n['language']['name']: n['name'] for n in d['names']}
        en = ascii_up(names['en'])
        dif = {}
        for lg in LANGS:
            v = ascii_up(names.get(lg, names['en']))
            if v != en:
                dif[lg] = v
        for lg in LANGS_UTF8:
            v = names.get(lg)
            if v:
                # 'ja-hrkt' -> 'ja'; el UTF-8 se respeta salvo ♀/♂, que la
                # fuente no tiene (ver sin_genero)
                dif[lg.split('-')[0]] = sin_genero(v)
        if dif:
            out[num] = dif
        if num % 25 == 0:
            print(f'  {num}/151...')
        time.sleep(0.05)

    path = os.path.join(os.path.dirname(__file__), 'dex_names.py')
    with open(path, 'w', encoding='utf-8') as f:
        f.write('# -*- coding: utf-8 -*-\n')
        f.write('"""GENERADO por tools/gen_names.py desde PokeAPI - no editar a mano.\n\n')
        f.write('Nombres oficiales por idioma. FR y DE van en mayusculas y sin\n')
        f.write('acentos (fuente CP437, un byte por caracter); JA va en katakana\n')
        f.write('UTF-8 tal cual, porque se pinta con una fuente U8g2.\n"""\n\n')
        f.write('LOCAL_NAMES = {\n')
        for num in sorted(out):
            f.write(f'    {num}: {out[num]!r},\n')
        f.write('}\n')
    print(f'guardado {os.path.normpath(path)}: {len(out)} especies con nombre propio')


if __name__ == '__main__':
    main()
