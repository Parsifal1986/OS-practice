#ifndef NCCL_BROADCAST_CUH
#define NCCL_BROADCAST_CUH

#define CUDACHECK(cmd) do {                         \
  cudaError_t err = cmd;                            \
  if (err != cudaSuccess) {                         \
    printf("Failed: Cuda error %s:%d '%s'\n",       \
        __FILE__,__LINE__,cudaGetErrorString(err)); \
    exit(EXIT_FAILURE);                             \
  }                                                 \
} while(0)

#define NCCLCHECK(cmd) do {                         \
  ncclResult_t res = cmd;                           \
  if (res != ncclSuccess) {                         \
    printf("Failed, NCCL error %s:%d '%s'\n",       \
        __FILE__,__LINE__,ncclGetErrorString(res)); \
    exit(EXIT_FAILURE);                             \
  }                                                 \
} while(0)

#include <nccl.h>
#include <cuda_runtime.h>

ncclResult_t nccl_broadcast_data(void* data, size_t count, int root, ncclComm_t comm) {
  cudaStream_t stream;
  CUDACHECK(cudaStreamCreate(&stream));

  float* d_ptr;
  CUDACHECK(cudaMalloc((void **)&d_ptr, count * sizeof(float)));
  CUDACHECK(cudaMemcpyAsync(d_ptr, data, count * sizeof(float), cudaMemcpyHostToDevice, stream));
  NCCLCHECK(ncclBroadcast(d_ptr, d_ptr, count, ncclFloat, root, comm, stream));
  CUDACHECK(cudaStreamSynchronize(stream));
  CUDACHECK(cudaFree(d_ptr));
  return ncclSuccess;
}

ncclResult_t nccl_all_reduce(void* data, size_t count, ncclRedOp_t opt, ncclComm_t comm) {
  cudaStream_t stream;
  CUDACHECK(cudaStreamCreate(&stream));

  NCCLCHECK(ncclAllReduce(data, data, count, ncclFloat, opt, comm, stream));
  CUDACHECK(cudaStreamSynchronize(stream));
  return ncclSuccess;
}

#endif // NCCL_BROADCAST_CUH