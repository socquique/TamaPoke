#!/usr/bin/env python3
"""Tests de las herramientas del taller (tools/) y de los ficheros generados.

  python3 test/test_tools.py
"""
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(ROOT, 'tools')
sys.path.insert(0, TOOLS)


class TestToolsCompile(unittest.TestCase):
    def test_todos_los_scripts_compilan(self):
        scripts = sorted(f for f in os.listdir(TOOLS) if f.endswith('.py'))
        self.assertTrue(scripts, 'no se encontro ningun script en tools/')
        for name in scripts:
            with self.subTest(script=name):
                with open(os.path.join(TOOLS, name), encoding='utf-8') as fh:
                    src = fh.read()
                try:
                    compile(src, name, 'exec')
                except SyntaxError as e:
                    self.fail(f'{name} no compila: {e}')

    def test_los_scripts_ejecutables_declaran_shebang(self):
        for name in sorted(f for f in os.listdir(TOOLS) if f.endswith('.py')):
            path = os.path.join(TOOLS, name)
            if not os.access(path, os.X_OK):
                continue
            with open(path) as fh:
                self.assertTrue(fh.readline().startswith('#!'),
                                f'{name} es ejecutable pero no tiene shebang')


class TestDexData(unittest.TestCase):
    """Invariantes de la fuente de datos de la Pokedex."""

    @classmethod
    def setUpClass(cls):
        from dex_data import DEX, TYPE_ACCENTS, CLASSIC, RARE, LEGENDARY, SLUGS
        from dex_stats import BASE_STATS
        from dex_names import LOCAL_NAMES
        cls.DEX, cls.ACCENTS, cls.CLASSIC = DEX, TYPE_ACCENTS, CLASSIC
        cls.RARE, cls.LEGENDARY, cls.SLUGS = RARE, LEGENDARY, SLUGS
        cls.STATS, cls.NAMES = BASE_STATS, LOCAL_NAMES
        cls.byNum = {row[0]: row for row in DEX}

    def test_estan_las_151_especies_sin_huecos_ni_repetidos(self):
        nums = [row[0] for row in self.DEX]
        self.assertEqual(len(nums), 151)
        self.assertEqual(sorted(nums), list(range(1, 152)))

    def test_los_slugs_son_unicos_y_en_minusculas(self):
        slugs = [row[1] for row in self.DEX]
        self.assertEqual(len(set(slugs)), len(slugs), 'hay slugs repetidos')
        for s in slugs:
            self.assertEqual(s, s.lower(), f'slug con mayusculas: {s}')
            self.assertTrue(s.isascii(), f'slug no ASCII: {s}')

    def test_los_nombres_de_pantalla_son_ascii_en_mayusculas(self):
        # la fuente GFX del firmware no tiene acentos ni minusculas
        for num, slug, display, *_ in self.DEX:
            self.assertTrue(display.isascii(), f'{num} {display} no es ASCII')
            self.assertEqual(display, display.upper(), f'{num} {display} lleva minusculas')
            self.assertLessEqual(len(display), 12, f'{num} {display} no cabe en pantalla')

    def test_las_evoluciones_apuntan_a_especies_reales(self):
        for num, slug, display, tipo, evo, lvl in self.DEX:
            if evo:
                self.assertIn(evo, self.byNum, f'{display} evoluciona a {evo}, que no existe')
                self.assertNotEqual(evo, num, f'{display} evoluciona a si mismo')
                self.assertGreater(lvl, 0, f'{display} evoluciona sin nivel')
            else:
                self.assertEqual(lvl, 0, f'{display} es forma final pero tiene nivel {lvl}')

    def test_ninguna_cadena_evolutiva_hace_bucle(self):
        for num in self.byNum:
            seen, cur = set(), num
            while cur and self.byNum[cur][4]:
                self.assertNotIn(cur, seen, f'bucle evolutivo desde {num}')
                seen.add(cur)
                cur = self.byNum[cur][4]
                self.assertLessEqual(len(seen), 5, f'cadena demasiado larga desde {num}')

    def test_todos_los_tipos_tienen_color(self):
        for num, slug, display, tipo, *_ in self.DEX:
            self.assertIn(tipo, self.ACCENTS, f'{display}: tipo "{tipo}" sin color')

    def test_los_colores_son_hex_de_6_digitos(self):
        for tipo, col in self.ACCENTS.items():
            self.assertRegex(col, r'^#[0-9a-fA-F]{6}$', f'color raro en {tipo}: {col}')

    def test_las_rarezas_apuntan_a_formas_base(self):
        evolucionadas = {row[4] for row in self.DEX if row[4]}
        for num in sorted(self.RARE | self.LEGENDARY):
            self.assertIn(num, self.byNum, f'rareza para una especie inexistente: {num}')
            self.assertNotIn(num, evolucionadas,
                             f'{self.byNum[num][2]} solo sale por evolucion: la rareza no le sirve')
        self.assertFalse(self.RARE & self.LEGENDARY, 'una especie no puede ser rara y legendaria')

    def test_los_iniciales_clasicos_existen(self):
        evolucionadas = {row[4] for row in self.DEX if row[4]}
        for num in self.CLASSIC:
            self.assertIn(num, self.byNum, f'inicial inexistente: {num}')
            self.assertNotIn(num, evolucionadas, f'{self.byNum[num][2]} no puede ser inicial')

    def test_hay_stats_base_de_las_151(self):
        for num in self.byNum:
            self.assertIn(num, self.STATS, f'faltan stats de {self.byNum[num][2]}')
            hp, atk, dfn, spe = self.STATS[num]
            for v in (hp, atk, dfn, spe):
                self.assertTrue(0 < v <= 255, f'stat fuera de rango en {self.byNum[num][2]}: {v}')

    def test_los_nombres_traducidos_son_ascii_en_mayusculas(self):
        for num, langs in self.NAMES.items():
            self.assertIn(num, self.byNum, f'nombre traducido de una especie inexistente: {num}')
            for lang, name in langs.items():
                self.assertIn(lang, ('fr', 'de'), f'idioma inesperado: {lang}')
                self.assertTrue(name.isascii(), f'{num} {lang} no es ASCII: {name}')
                self.assertEqual(name, name.upper(), f'{num} {lang} lleva minusculas: {name}')
                self.assertLessEqual(len(name), 12, f'{num} {lang} no cabe en pantalla: {name}')


class TestGeneratedFiles(unittest.TestCase):
    """dex.h esta generado: si no coincide con su generador, alguien lo edito a mano."""

    def test_gen_dex_reproduce_dex_h(self):
        tmp = tempfile.mkdtemp(prefix='tamapoke-gen-')
        try:
            shutil.copytree(TOOLS, os.path.join(tmp, 'tools'))
            r = subprocess.run([sys.executable, os.path.join(tmp, 'tools', 'gen_dex.py')],
                               capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, f'gen_dex.py fallo:\n{r.stderr}')
            generated = os.path.join(tmp, 'dex.h')
            self.assertTrue(os.path.exists(generated), 'gen_dex.py no escribio dex.h')
            with open(generated) as a, open(os.path.join(ROOT, 'dex.h')) as b:
                self.assertEqual(a.read(), b.read(),
                                 'dex.h no coincide con lo que genera tools/gen_dex.py '
                                 '(vuelve a generarlo en vez de editarlo a mano)')
        finally:
            shutil.rmtree(tmp, ignore_errors=True)

    def test_los_ficheros_generados_avisan_de_que_lo_son(self):
        for name in ('dex.h', 'species.h'):
            with open(os.path.join(ROOT, name)) as fh:
                head = fh.read(400)
            self.assertIn('GENERADO', head, f'{name} deberia decir que esta generado')


class TestSketchSanity(unittest.TestCase):
    """Comprobaciones baratas sobre el .ino que no necesitan compilarlo."""

    @classmethod
    def setUpClass(cls):
        with open(os.path.join(ROOT, 'TamaPoke.ino')) as fh:
            cls.ino = fh.read()

    def test_el_sketch_no_lleva_caracteres_no_ascii_en_literales_de_pantalla(self):
        # los comentarios pueden llevar lo que quieran; los literales, no:
        # la fuente GFX es ASCII y un acento sale como basura
        for lineno, line in enumerate(self.ino.splitlines(), 1):
            code = line.split('//')[0]
            for chunk in code.split('"')[1::2]:
                self.assertTrue(chunk.isascii(),
                                f'TamaPoke.ino:{lineno}: literal no ASCII: {chunk!r}')

    def test_las_llaves_estan_equilibradas(self):
        self.assertEqual(self.ino.count('{'), self.ino.count('}'),
                         'llaves descompensadas en TamaPoke.ino')


if __name__ == '__main__':
    unittest.main(verbosity=2)
