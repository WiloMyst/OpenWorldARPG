#!/bin/bash
ls /mnt/d/Triton/OpenWorldARPG/VHServer/models/ 2>/dev/null || echo "NO MODELS DIR"
python3 -c 'import grpc; print("grpcio ok")' 2>&1
python3 -c 'import grpc_tools; print("grpcio-tools ok")' 2>&1
