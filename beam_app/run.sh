#!/bin/bash

echo "==============================="
echo "Activando entorno virtual..."
echo "==============================="

if [ ! -d "venv" ]; then
    echo "No existe el entorno virtual 'venv'."
    echo "Crealo primero con:"
    echo "python3 -m venv venv"
    exit 1
fi

source venv/bin/activate

echo "==============================="
echo "Ejecutando aplicacion..."
echo "==============================="

python main.py