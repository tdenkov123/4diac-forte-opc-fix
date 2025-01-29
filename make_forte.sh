#!/bin/bash

# Vars
FORTE_URL="https://github.com/tdenkov123/4diac-forte-opc-fix"
FORTE_BRANCH="freeze"
OPEN62541_URL="https://github.com/open62541/open62541.git"
OPEN62541_BRANCH="1.4"

mkdir forte-opc-fix
cd forte-opc-fix

# Building open62541
git clone $OPEN62541_URL open62541
cd open62541
git checkout $OPEN62541_BRANCH
mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Debug -DUA_ENABLE_AMALGAMATION=ON -DUA_ENABLE_MQTT=ON .. 
make
cd ../../

# Building Forte
git clone $FORTE_URL forte
cd forte
git checkout $FORTE_BRANCH
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DFORTE_ARCHITECTURE=Posix -DFORTE_MODULE_CONVERT=ON -DFORTE_COM_ETH=ON -DFORTE_MODULE_IEC61131=ON -DFORTE_COM_OPC_UA=ON -DFORTE_COM_OPC_UA_INCLUDE_DIR=$PWD/../../open62541/build -DFORTE_COM_OPC_UA_LIB_DIR=$PWD/../../open62541/build/bin -DFORTE_COM_OPC_UA_LIB=libopen62541.so -DFORTE_DEVICE=OPCUA_DEV -DFORTE_LOGLEVEL=LOGERROR ..
make
cd src

echo Building forte finished