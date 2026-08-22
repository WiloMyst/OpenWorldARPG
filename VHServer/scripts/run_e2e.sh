#!/bin/bash
set -e
cd /mnt/d/Triton/OpenWorldARPG/VHServer/scripts

# 生成 Python pb (与服务器同一份 proto)
python3 -m grpc_tools.protoc -I../protos -I../../GameServer/protos \
    --python_out=. --grpc_python_out=. \
    ../protos/avatarStream.proto ../../GameServer/protos/game.proto

python3 e2e_dialogue_auth.py
