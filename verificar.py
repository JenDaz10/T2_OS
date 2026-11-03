#!/usr/bin/env python3
"""Pruebas funcionales básicas, no sustituye el suite de evaluación del docente."""
import pathlib
import re
import subprocess
import signal
import time

ROOT = pathlib.Path(__file__).resolve().parent

def ejecutar(*argumentos):
    p = subprocess.run([str(ROOT / 'torneo'), *map(str, argumentos)], cwd=ROOT,
                       capture_output=True, text=True, timeout=15)
    assert p.returncode == 0, p.stderr
    return p.stdout

def contar(s, texto):
    return s.count(texto)

def comprobar_limite_mesas(log, limite):
    activas = set()
    max_observado = 0
    for linea in log.splitlines():
        inicio = re.search(r'\[Mesa (\d+)\] Juez inicia:', linea)
        final = re.search(r'\[Mesa (\d+)\] FINAL:', linea)
        if inicio:
            numero = int(inicio.group(1))
            assert numero not in activas
            activas.add(numero)
            max_observado = max(max_observado, len(activas))
        if final:
            numero = int(final.group(1))
            assert numero in activas
            activas.remove(numero)
    assert max_observado <= limite, (max_observado, limite)
    assert not activas
    return max_observado

subprocess.run(['make', 'test'], cwd=ROOT, check=True)
for n, m in [(20, 2), (50, 4), (3, 1)]:
    out=ejecutar(n, m, 'config_rapida.txt')
    esperado=n//2
    assert contar(out, 'Juez inicia:') == esperado
    assert contar(out, 'FINAL:') == esperado
    maximo = comprobar_limite_mesas(out, m)
    assert ('sin pareja (N impar)' in out) == (n%2==1)
    print(f'[OK] {n} clientes, {m} mesas, {esperado} partidas; maximo simultaneo: {maximo}')

out=ejecutar(2, 1, 'config_desconexion.txt')
assert 'PERROS ganan por abandono del cliente Puma' in out
print('[OK] desconexion Puma detectada')

config=ROOT / 'config_desconexion_perros.txt'
out=ejecutar(2, 1, config.name)
assert 'PUMA gana por abandono del cliente Perros' in out
print('[OK] desconexion Perros detectada')

p=subprocess.run([str(ROOT/'torneo'),'2','1','test_cases.txt'],cwd=ROOT,capture_output=True,text=True,timeout=5)
assert p.returncode != 0 and 'Clave desconocida' in p.stderr
print('[OK] suite no confundida con configuracion')

p=subprocess.Popen([str(ROOT/'torneo'),'20','2','config.txt'],cwd=ROOT,
                   stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
time.sleep(0.2)
p.send_signal(signal.SIGINT)
out,err=p.communicate(timeout=8)
assert p.returncode==0, err
assert contar(out, 'Juez inicia:')==contar(out, 'FINAL:')
assert 'CANCELADA' in out and 'Recursos POSIX liberados' in out
print('[OK] SIGINT cancela jueces activos y libera recursos')
print('\nPruebas integradas completadas.')
