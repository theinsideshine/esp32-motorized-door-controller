@echo off

echo ===============================
echo Activando entorno virtual...
echo ===============================

if not exist .venv (
    echo Creando entorno virtual...
    py -3.11 -m venv .venv
)

call .venv\Scripts\activate

echo ===============================
echo Ejecutando aplicacion...
echo ===============================

python main.py

pause