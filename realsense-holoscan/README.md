```
pip install --force-reinstall "numpy<2"
pip install nvidia-cuda-runtime-cu12
export CUDA_WHL_LIB_DIR=$(python3 -c 'import nvidia.cuda_runtime; print(nvidia.cuda_runtime.__path__[0])')/lib
export LD_LIBRARY_PATH="$CUDA_WHL_LIB_DIR:$LD_LIBRARY_PATH"

python3 realsense.py
```