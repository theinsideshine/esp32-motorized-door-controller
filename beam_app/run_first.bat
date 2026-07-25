@echo off
echo === SETUP INICIAL ===

if not exist .venv (
echo Creando entorno virtual...
py -3.11 -m venv .venv
)

echo Activando entorno...
call .venv\Scripts\activate

echo Actualizando pip...
python -m pip install --upgrade pip

echo Instalando dependencias...
pip install -r requirements.txt

echo === LISTO ===
pause
