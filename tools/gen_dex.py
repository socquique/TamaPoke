#!/usr/bin/env python3
"""Genera dex.h (tabla de la Pokedex para el firmware) desde dex_data.py.

  python3 tools/gen_dex.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from dex_data import DEX, TYPE_ACCENTS, BATTLE_TYPES, CLASSIC, RARE, LEGENDARY
from dex_stats import BASE_STATS


def rgb565(hexcol):
    r, g, b = int(hexcol[1:3], 16), int(hexcol[3:5], 16), int(hexcol[5:7], 16)
    return (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)


# bioma de fondo por tipo (la luz la pone la hora real del RTC)
# 0 PRADERA, 1 PLAYA, 2 BOSQUE, 3 VOLCAN, 4 MONTANA, 5 NIEVE
TYPE_BIOME = {
    'agua': 1, 'planta': 2, 'bicho': 2, 'fuego': 3,
    'roca': 4, 'tierra': 4, 'dragon': 1, 'hielo': 5,  # los dragones gen1 (Dratini) viven en el agua
    'normal': 0, 'electrico': 0, 'lucha': 0, 'veneno': 0,
    'psiquico': 0, 'fantasma': 0,
}

# excepciones por dex# (el tipo no basta): fosiles marinos roca/agua -> playa
BIOME_OVERRIDE = {138: 1, 139: 1, 140: 1, 141: 1}  # Omanyte, Omastar, Kabuto, Kabutops

BATTLE_TYPE_IDS = {
    'none': 0,
    'normal': 1,
    'fire': 2,
    'water': 3,
    'electric': 4,
    'grass': 5,
    'ice': 6,
    'fighting': 7,
    'poison': 8,
    'ground': 9,
    'flying': 10,
    'psychic': 11,
    'bug': 12,
    'rock': 13,
    'ghost': 14,
    'dragon': 15,
    'dark': 16,
    'steel': 17,
    'fairy': 18,
}


def main():
    out = []
    out.append("#pragma once\n#include <stdint.h>\n\n")
    out.append("// GENERADO por tools/gen_dex.py desde tools/dex_data.py - no editar\n\n")
    out.append("#define DEX_COUNT 151\n")
    out.append("#define DEX_EEVEE 133  // rama al azar: 134/135/136\n\n")
    out.append(
        "// rareza: 0 = solo por evolucion, 1 = comun, 2 = raro, 3 = legendario\n"
        "enum : uint8_t { R_EVO = 0, R_COMUN, R_RARO, R_LEGENDARIO };\n\n"
        "// tipos de combate: datos actuales de PokeAPI para las 151 especies Kanto\n"
        "enum : uint8_t {\n"
        "  TYPE_NONE = 0, TYPE_NORMAL, TYPE_FIRE, TYPE_WATER, TYPE_ELECTRIC, TYPE_GRASS,\n"
        "  TYPE_ICE, TYPE_FIGHTING, TYPE_POISON, TYPE_GROUND, TYPE_FLYING, TYPE_PSYCHIC,\n"
        "  TYPE_BUG, TYPE_ROCK, TYPE_GHOST, TYPE_DRAGON, TYPE_DARK, TYPE_STEEL, TYPE_FAIRY\n"
        "};\n\n"
        "struct DexEntry {\n"
        "  const char *name;\n"
        "  uint8_t evolvesTo;    // numero de dex, 0 = forma final\n"
        "  uint8_t evolveLevel;\n"
        "  uint8_t rarity;       // sale de huevo si > 0\n"
        "  uint16_t accent;      // color RGB565 del tipo para la UI\n"
        "  uint8_t bHp, bAtk, bDef, bSpe;  // base stats reales de gen 1\n"
        "  uint8_t type1, type2; // tipos de combate, TYPE_NONE si no hay secundario\n"
        "  uint8_t biome;        // 0 pradera 1 playa 2 bosque 3 volcan 4 montana 5 nieve\n"
        "};\n\n")
    # formas base = las que no son evolucion de nadie (las ramas de Eevee si lo son)
    evolved = {evo for *_, evo, _lvl in [(d[4], d[5]) for d in DEX] for evo in [_[0] for _ in [(d[4],) for d in DEX]]}
    evolved = {d[4] for d in DEX if d[4]} | {135, 136}
    rarities = []
    out.append("static const DexEntry DEX_TBL[DEX_COUNT + 1] = {\n")
    out.append('  { "?", 0, 0, 0, 0x2946, 50, 50, 50, 50, TYPE_NONE, TYPE_NONE, 0 },  // 0: sin usar\n')
    for num, slug, display, typ, evo, lvl in DEX:
        acc = rgb565(TYPE_ACCENTS[typ])
        if num in evolved:
            rar = 'R_EVO'
        elif num in LEGENDARY:
            rar = 'R_LEGENDARIO'
        elif num in RARE:
            rar = 'R_RARO'
        else:
            rar = 'R_COMUN'
        rarities.append(rar)
        hp, atk, df, spe = BASE_STATS[num]
        bio = BIOME_OVERRIDE.get(num, TYPE_BIOME[typ])
        type1, type2 = BATTLE_TYPES[num]
        t1 = f"TYPE_{type1.upper()}"
        t2 = "TYPE_NONE" if type2 is None else f"TYPE_{type2.upper()}"
        out.append(f'  {{ "{display}", {evo}, {lvl}, {rar}, 0x{acc:04X}, {hp}, {atk}, {df}, {spe}, {t1}, {t2}, {bio} }},  // {num} {type1}' + (f'/{type2}' if type2 else '') + '\n')
    out.append("};\n\n")
    out.append("// el primer huevo de la partida: iniciales clasicos\n")
    out.append("static const int16_t CLASSIC_DEX[] = { %s };\n" % ", ".join(map(str, CLASSIC)))
    out.append(f"#define NUM_CLASSIC_DEX {len(CLASSIC)}\n")
    from collections import Counter
    c = Counter(rarities)
    print(f"bases: {c['R_COMUN']} comunes, {c['R_RARO']} raras, {c['R_LEGENDARIO']} legendarias, {c['R_EVO']} solo-evolucion")

    path = os.path.join(os.path.dirname(__file__), '..', 'dex.h')
    open(path, 'w').write(''.join(out))
    print(f"guardado {os.path.normpath(path)} ({len(DEX)} especies)")


if __name__ == '__main__':
    main()
