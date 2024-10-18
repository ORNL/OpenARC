#include "openacc.h"
#include "openaccrt_ext.h"
#include <iris/rt/DeviceCUDA.h>
#include <iris/rt/LoaderCUDA.h>
#include <iris/rt/Mem.h>
#include <iris/rt/Command.h>

//Below macro is used to configure task behaviors assigned to the default queue.
//Set IRIS_TASK_SUBMIT_MODE = 0 to submit a task synchronously
//                            1 to submit a task asynchronously if the task contains only a kernel
#if VICTIM_CACHE_MODE == 0
//If VICTIM_CACHE_MODE == 0, IRIS_TASK_SUBMIT_MODE should be set to 0.
#define IRIS_TASK_SUBMIT_MODE 0
#else
//If VICTIM_CACHE_MODE == 1, IRIS_TASK_SUBMIT_MODE can be set to 0 or 1.
#define IRIS_TASK_SUBMIT_MODE 1
#endif
//Below macro is used to optimize IRIS tasks with memory transfers only, when users choose to use IRIS policy.
//Set OPT_MEMCPY_ONLY_POLICY = 2 to apply iris policy to IRIS tasks with H2D memory transfers only 
//                               and iris policy to IRIS tasks with D2H memory transfers only
//    OPT_MEMCPY_ONLY_POLICY = 1 to apply iris policy to IRIS tasks with D2H memory transfers only 
//    OPT_MEMCPY_ONLY_POLICY = 0 to apply user-specified policy to all IRIS tasks.
#define OPT_MEMCPY_ONLY_POLICY 2

static const char *openarcrt_iris_policy_env = "OPENARCRT_IRIS_POLICY";
static const char *openarcrt_iris_dmem_env = "OPENARCRT_IRIS_DMEM";
static const char *iris_policy_roundrobin = "iris_roundrobin";
static const char *iris_policy_depend = "iris_depend";
static const char *iris_policy_data = "iris_data";
static const char *iris_policy_profile = "iris_profile";
static const char *iris_policy_random = "iris_random";
static const char *iris_policy_pending = "iris_pending";
static const char *iris_policy_sdq = "iris_sdq";
static const char *iris_policy_ftf = "iris_ftf";
static const char *iris_policy_custom = "iris_custom";

//Below structures contain IRIS device IDs for a given device type.
std::vector<int> IrisDriver::NVIDIADeviceIDs;
std::vector<int> IrisDriver::AMDDeviceIDs;
std::vector<int> IrisDriver::GPUDeviceIDs;
std::vector<int> IrisDriver::CPUDeviceIDs;
std::vector<int> IrisDriver::FPGADeviceIDs;
std::vector<int> IrisDriver::PhiDeviceIDs;
std::vector<int> IrisDriver::DefaultDeviceIDs;
int IrisDriver::openarcrt_iris_policy;
int IrisDriver::openarcrt_iris_dmem;

int IrisDriver::HI_getIrisDeviceID(acc_device_t devType, acc_device_t userInput, int devnum, int memTrOnly) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_getIrisDeviceID(%s, %d)\n", HI_get_device_type_string(devType), devnum);
    }    
#endif
	int irisDeviceType = iris_default;
	int irisDeviceID = devnum;
	if( openarcrt_iris_policy != 0 ) {
#ifdef _OPENARC_PROFILE_
    	if( HI_openarcrt_verbosity > 2 ) {
        	fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_getIrisDeviceID(%s, %d)\n", HI_get_device_type_string(devType), devnum);
    	}    
#endif
		if( memTrOnly == 2 ) {
			//If an IRIS task contains H2D memory transfers only, iris_pending policy is used to minimize unnecessary memory transfers between devices.
			return iris_pending;
		} else if( memTrOnly == 1 ) {
			//If an IRIS task contains D2H memory transfers only, iris_data policy is used to minimize unnecessary memory transfers between devices.
			return iris_data;
		} else {
			return openarcrt_iris_policy;
		}
	}
	switch (devType) {
		case acc_device_default: { irisDeviceType = iris_default; break; }
		case acc_device_host: { irisDeviceType = iris_cpu; break; }
		case acc_device_not_host: { irisDeviceType = iris_default; break; }
		case acc_device_nvidia: { irisDeviceType = iris_nvidia; break; }
		case acc_device_radeon: { irisDeviceType = iris_amd; break; }
		case acc_device_gpu: { if( userInput == acc_device_nvidia ) {irisDeviceType = iris_nvidia;} 
								else if( userInput == acc_device_radeon ) {irisDeviceType = iris_amd;}
								else {irisDeviceType = iris_gpu;}
								break; }
		case acc_device_xeonphi: { irisDeviceType = iris_phi; break; }
		case acc_device_current: { irisDeviceType = iris_default; break; }
		case acc_device_altera: { irisDeviceType = iris_fpga; break; }
		case acc_device_altera_emulator: { irisDeviceType = iris_fpga; break; }
		default: { irisDeviceType = iris_default; break; }
	}
	switch (irisDeviceType) {
		case iris_default: { if( devnum >= DefaultDeviceIDs.size() ) {
      									fprintf(stderr, "[ERROR in IrisDriver::HI_getIrisDeviceID()] device number (%d) is greater than the number of available devices (%lu) of a given type (iris_default).\n",devnum, DefaultDeviceIDs.size());
      									exit(1);
									} else {
										irisDeviceID = DefaultDeviceIDs[devnum];
									}
									break; }
		case iris_nvidia: { if( devnum >= NVIDIADeviceIDs.size() ) {
      									fprintf(stderr, "[ERROR in IrisDriver::HI_getIrisDeviceID()] device number (%d) is greater than the number of available devices (%lu) of a given type (iris_nvidia).\n",devnum, NVIDIADeviceIDs.size());
      									exit(1);
									} else {
										irisDeviceID = NVIDIADeviceIDs[devnum];
									}
									break; }
		case iris_amd: { if( devnum >= AMDDeviceIDs.size() ) {
      									fprintf(stderr, "[ERROR in IrisDriver::HI_getIrisDeviceID()] device number (%d) is greater than the number of available devices (%lu) of a given type (iris_amd).\n",devnum, AMDDeviceIDs.size());
      									exit(1);
									} else {
										irisDeviceID = AMDDeviceIDs[devnum];
									}
									break; }
		case iris_gpu: { if( devnum >= GPUDeviceIDs.size() ) {
      									fprintf(stderr, "[ERROR in IrisDriver::HI_getIrisDeviceID()] device number (%d) is greater than the number of available devices (%lu) of a given type (iris_gpu).\n",devnum, GPUDeviceIDs.size());
      									exit(1);
									} else {
										irisDeviceID = GPUDeviceIDs[devnum];
									}
									break; }
		case iris_phi: { if( devnum >= PhiDeviceIDs.size() ) {
      									fprintf(stderr, "[ERROR in IrisDriver::HI_getIrisDeviceID()] device number (%d) is greater than the number of available devices (%lu) of a given type (iris_phi).\n",devnum, PhiDeviceIDs.size());
      									exit(1);
									} else {
										irisDeviceID = PhiDeviceIDs[devnum];
									}
									break; }
		case iris_fpga: { if( devnum >= FPGADeviceIDs.size() ) {
      									fprintf(stderr, "[ERROR in IrisDriver::HI_getIrisDeviceID()] device number (%d) is greater than the number of available devices (%lu) of a given type (iris_fpga).\n",devnum, FPGADeviceIDs.size());
      									exit(1);
									} else {
										irisDeviceID = FPGADeviceIDs[devnum];
									}
									break; }
		case iris_cpu: { if( devnum >= CPUDeviceIDs.size() ) {
      									fprintf(stderr, "[ERROR in IrisDriver::HI_getIrisDeviceID()] device number (%d) is greater than the number of available devices (%lu) of a given type (iris_cpu).\n",devnum, CPUDeviceIDs.size());
      									exit(1);
									} else {
										irisDeviceID = CPUDeviceIDs[devnum];
									}
									break; }
		default: { if( devnum >= DefaultDeviceIDs.size() ) {
      									fprintf(stderr, "[ERROR in IrisDriver::HI_getIrisDeviceID()] device number (%d) is greater than the number of available devices (%lu) of a given type (iris_default).\n",devnum, DefaultDeviceIDs.size());
      									exit(1);
									} else {
										irisDeviceID = DefaultDeviceIDs[devnum];
									}
									break; }
	}
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_getIrisDeviceID(%s, %d)\n", HI_get_device_type_string(devType), devnum);
    }    
#endif
	return irisDeviceID;
}

IrisDriver::IrisDriver(acc_device_t devType, int devNum, std::set<std::string>kernelNames, HostConf_t *conf, int numDevices, const char *baseFileName) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::IrisDriver(%s, %d)\n", HI_get_device_type_string(devType), devNum);
    }    
#endif
  current_mempool_size = 0;
  fileNameBase = std::string(baseFileName);

#ifdef _THREAD_SAFETY
  pthread_mutex_lock(&mutex_set_device_num);
#else
#ifdef _OPENMP
  #pragma omp critical(acc_set_device_num_critical)
#endif
#endif
  for (std::set<std::string>::iterator it = kernelNames.begin() ; it != kernelNames.end(); ++it) {
    kernelNameSet.insert(*it);
  }
#ifdef _THREAD_SAFETY
  pthread_mutex_unlock(&mutex_set_device_num);
#endif
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::IrisDriver(%s, %d)\n", HI_get_device_type_string(devType), devNum);
    }    
#endif
}

int bind_tex_handler(void* p, void* device) {
  char* params = (char*) p;
  int off = 0;
  size_t name_len = *((size_t*) params);
  off += sizeof(name_len);
  char* name = (char*) malloc(name_len);
  memcpy(name, params + off, name_len);
  off += name_len;
  int type = *((int*) (params + off));
  off += sizeof(type);
  iris_mem mHandle = *((iris_mem*) (params + off));
  off += sizeof(mHandle);
  size_t size = *((size_t*) (params + off));
  //printf("[%s:%d] namelen[%lu] name[%s] type[%d] mHandle[%d] size[%lu]\n", __FILE__, __LINE__, name_len, name, type, mHandle, size);

  CUtexref tex;
  CUresult err;
  iris_mem m = (iris_mem) mHandle;
  iris::rt::DeviceCUDA* dev = (iris::rt::DeviceCUDA*) device;
  iris::rt::LoaderCUDA* ld = dev->ld();
  iris::rt::Mem* mem = (iris::rt::Mem*)m.class_obj;
  err = ld->cuModuleGetTexRef(&tex, *(dev->module()), name);
  if (err != CUDA_SUCCESS) printf("[%s:%d] err[%d]\n", __FILE__, __LINE__, err);
  err = ld->cuTexRefSetAddress(0, tex, (CUdeviceptr) mem->arch(dev), size);
  if (err != CUDA_SUCCESS) printf("[%s:%d] err[%d]\n", __FILE__, __LINE__, err);
  err = ld->cuTexRefSetAddressMode(tex, 0, CU_TR_ADDRESS_MODE_WRAP);
  if (err != CUDA_SUCCESS) printf("[%s:%d] err[%d]\n", __FILE__, __LINE__, err);
  err = ld->cuTexRefSetFilterMode(tex, CU_TR_FILTER_MODE_LINEAR);
  if (err != CUDA_SUCCESS) printf("[%s:%d] err[%d]\n", __FILE__, __LINE__, err);
  err = ld->cuTexRefSetFlags(tex, CU_TRSF_NORMALIZED_COORDINATES);
  if (err != CUDA_SUCCESS) printf("[%s:%d] err[%d]\n", __FILE__, __LINE__, err);
  err = ld->cuTexRefSetFormat(tex, type == iris_int ? CU_AD_FORMAT_SIGNED_INT32 : CU_AD_FORMAT_FLOAT, 1);
  if (err != CUDA_SUCCESS) printf("[%s:%d] err[%d]\n", __FILE__, __LINE__, err);
  return 0;
}

HI_error_t IrisDriver::init(int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::init()\n");
    }
#endif

  current_mempool_size = 0;
  HostConf_t * tconf = getHostConf(threadID);
  int thread_id = tconf->threadID;
  unifiedMemSupported = 0;

  int err;
  //[DEBUG on May 13, 2021] below initialization is not needed since it will be done in HI_get_num_devices_init()
  //and there is only one IRIS device when targeting IRIS backend.
/*
  err = iris_init(NULL, NULL, 1);
  if (err == IRIS_SUCCESS) iris_register_command(0xdeadcafe, iris_nvidia, bind_tex_handler);
  int ndevs = 0;
  err = iris_device_count(&ndevs);
  if (err != IRIS_SUCCESS) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
  int defaultType = iris_default;
  for(int i=0; i<ndevs; i++) {
  	int type;
  	size_t size;
    err = iris_device_info(i, iris_type, &type, &size);  
  	if (err != IRIS_SUCCESS) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
    if( i==0 ) {
      defaultType = type;
    }
    if( type == defaultType ) {
      DefaultDeviceIDs.push_back(i);
    }
    if( type == iris_nvidia ) {
      NVIDIADeviceIDs.push_back(i);
      GPUDeviceIDs.push_back(i);
    } else if( type == iris_amd ) {
      AMDDeviceIDs.push_back(i);
      GPUDeviceIDs.push_back(i);
    } else if( type == iris_gpu ) {
      GPUDeviceIDs.push_back(i);
    } else if( type == iris_cpu ) {
      CPUDeviceIDs.push_back(i);
    } else if( type == iris_fpga ) {
      FPGADeviceIDs.push_back(i);
    } else if( type == iris_phi ) {
      PhiDeviceIDs.push_back(i);
    }
  }
  //Set IRIS policy if existing.
  const char * envVar = getenv(openarcrt_iris_policy_env);
  if( envVar == NULL ) {
	openarcrt_iris_policy = 0;
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity >= 0 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tIRIS Policy: none\n");
    }
#endif
  } else {
	if( strcmp(envVar, iris_policy_roundrobin) == 0 ) {
		openarcrt_iris_policy = iris_roundrobin;
	} else if( strcmp(envVar, iris_policy_depend) == 0 ) {
		openarcrt_iris_policy = iris_depend;
	} else if( strcmp(envVar, iris_policy_data) == 0 ) {
		openarcrt_iris_policy = iris_data;
	} else if( strcmp(envVar, iris_policy_profile) == 0 ) {
		openarcrt_iris_policy = iris_profile;
	} else if( strcmp(envVar, iris_policy_random) == 0 ) {
		openarcrt_iris_policy = iris_random;
	} else if( strcmp(envVar, iris_policy_sdq) == 0 ) {
		openarcrt_iris_policy = iris_sdq;
	} else if( strcmp(envVar, iris_policy_ftf) == 0 ) {
		openarcrt_iris_policy = iris_ftf;
	} else if( strcmp(envVar, iris_policy_custom) == 0 ) {
		openarcrt_iris_policy = iris_custom;
	} else {
		openarcrt_iris_policy = 0;
		envVar = "none";
	}
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity >= 0 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tIRIS Policy: %s\n", envVar);
    }
#endif
  }
*/

  masterAddressTableMap[thread_id] = new addresstable_t();
  masterHandleTable[thread_id] = new memhandlemap_t();
  postponedFreeTableMap[thread_id] = new asyncfreetable_t();
  postponedTempFreeTableMap[thread_id] = new asynctempfreetable_t();
  postponedTempFreeTableMap2[thread_id] = new asynctempfreetable2_t();
  memPoolMap[thread_id] = new memPool_t();
  tempMemPoolMap[thread_id] = new memPool_t();
  tempMallocSizeMap[thread_id] = new sizemap_t();
  threadAsyncMap[thread_id] = NO_QUEUE;
  //threadTaskMap[thread_id] = NULL;
  threadAsyncTaskMap[thread_id] = new threadtaskmapiris_t();
  threadTaskMapNesting[thread_id] = 0;
  threadHostMemFreeMap[thread_id] = new pointerset_t();

  createKernelArgMap(thread_id);
  init_done = 1;
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::init()\n");
    }
#endif
  return HI_success;
}

HI_error_t IrisDriver::HI_register_kernels(std::set<std::string>kernelNames, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_register_kernels(thread ID = %d)\n", threadID);
    }
#endif
#ifdef _THREAD_SAFETY
  pthread_mutex_lock(&mutex_set_device_num);
#else
#ifdef _OPENMP
  #pragma omp critical(acc_set_device_num_critical)
#endif
#endif
  {
  HostConf_t * tconf = getHostConf(threadID);
  for (std::set<std::string>::iterator it = kernelNames.begin() ; it != kernelNames.end(); ++it) {
    if( kernelNameSet.count(*it) == 0 ) {
      int err;
      iris_kernel kernel;
      const char *kernelName = (*it).c_str();
      err = iris_kernel_create(kernelName, &kernel);
      if (err != IRIS_SUCCESS) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
      (tconf->kernelsMap[this])[*it] = kernel;
      kernelParams_t *kernelParams = new kernelParams_t;
      kernelParams->num_args = 0;
      (tconf->kernelArgsMap[this]).insert(std::pair<std::string, kernelParams_t*>(std::string(kernelName), kernelParams));
    }
  }
  for (std::set<std::string>::iterator it = kernelNames.begin() ; it != kernelNames.end(); ++it) {
    if( kernelNameSet.count(*it) == 0 ) {
      kernelNameSet.insert(*it);
    }
  }
  }
#ifdef _THREAD_SAFETY
  pthread_mutex_unlock(&mutex_set_device_num);
#endif
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_register_kernels(thread ID = %d)\n", threadID);
    }
#endif
  return HI_success;
}

HI_error_t IrisDriver::HI_register_kernel_numargs(std::string kernel_name, int num_args, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_register_kernel_numargs(thread ID = %d)\n", threadID);
    }
#endif
  HostConf_t *tconf = getHostConf(threadID);
  kernelParams_t *kernelParams = tconf->kernelArgsMap.at(this).at(kernel_name);
  if( kernelParams->num_args == 0 ) {
    if( num_args > 0 ) {
      kernelParams->num_args = num_args;
      kernelParams->kernelParams = (void**)malloc(sizeof(void*) * num_args);
      kernelParams->kernelParamsOffset = (size_t*)malloc(sizeof(size_t) * num_args);
      kernelParams->kernelParamsInfo = (int*)malloc(sizeof(int) * num_args);
      kernelParams->kernelParamMems = (iris_mem*)malloc(sizeof(iris_mem) * num_args);
    } else {
      fprintf(stderr, "[ERROR in IrisDriver::HI_register_kernel_numargs(%s, %d)] num_args should be greater than zero.\n",kernel_name.c_str(), num_args);
      exit(1);
    }
  }
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_register_kernel_numargs(thread ID = %d)\n", threadID);
    }
#endif
  return HI_success;
}

HI_error_t IrisDriver::HI_register_kernel_arg(std::string kernel_name, int arg_index, size_t arg_size, void *arg_value, int arg_type, int arg_trait, size_t unitSize, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_register_kernel_arg(thread ID = %d)\n", threadID);
    }
    if( HI_openarcrt_verbosity > 4 ) {
  		fprintf(stderr, "[%s:%d][%s]\n", __FILE__, __LINE__, __func__);
	}
#endif
  HostConf_t *tconf = getHostConf(threadID);
  kernelParams_t * kernelParams = tconf->kernelArgsMap.at(this).at(kernel_name);
  int err;
  if( arg_type == 0 ) {
    *(kernelParams->kernelParams + arg_index) = arg_value;
    *(kernelParams->kernelParamsOffset + arg_index) = 0;
    *(kernelParams->kernelParamsInfo + arg_index) = (int)arg_size;
     iris_mem iMem;
    *(kernelParams->kernelParamMems + arg_index) = iMem;
    //err = iris_kernel_setarg((iris_kernel)(tconf->kernelsMap.at(this).at(kernel_name)), arg_index, arg_size, arg_value);
  } else {
    HI_device_mem_handle_t tHandle;
    if( HI_get_device_mem_handle(*((void **)arg_value), &tHandle, tconf->threadID) == HI_success ) {
      *(kernelParams->kernelParams + arg_index) = NULL;
      //*(kernelParams->kernelParamsOffset + arg_index) = 0 - tHandle.offset;
      //*(kernelParams->kernelParamsOffset + arg_index) = tHandle.offset/unitSize;
      *(kernelParams->kernelParamsOffset + arg_index) = tHandle.offset;
      if( arg_trait == 0 ) { //read-only
      	*(kernelParams->kernelParamsInfo + arg_index) = iris_r;
      } else if( arg_trait == 1 ) { //write-only
      	*(kernelParams->kernelParamsInfo + arg_index) = iris_w;
      } else if( arg_trait == 2 ) { //read-write
      	*(kernelParams->kernelParamsInfo + arg_index) = iris_rw;
      } else { //unknown or temporary
      	*(kernelParams->kernelParamsInfo + arg_index) = 0;
      }
      *(kernelParams->kernelParamMems + arg_index) = tHandle.memHandle;
      //err = iris_kernel_setmem_off((iris_kernel)(tconf->kernelsMap.at(this).at(kernel_name)), arg_index, *((iris_mem*) &(tHandle.memHandle)), tHandle.offset, iris_rw);
      //if (err != IRIS_SUCCESS) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
    } else {
		fprintf(stderr, "[ERROR in IrisDriver::HI_register_kernel_arg()] Cannot find a device pointer to memory handle mapping; failed to add argument %d to kernel %s (IRIS Device)\n", arg_index, kernel_name.c_str());
#ifdef _OPENARC_PROFILE_
		HI_print_device_address_mapping_entries(tconf->threadID);
#endif
		exit(1);
	}
  }
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_register_kernel_arg(thread ID = %d)\n", threadID);
    }
#endif
  return HI_success;
}

HI_error_t IrisDriver::HI_kernel_call(std::string kernel_name, size_t gridSize[3], size_t blockSize[3], int async, int num_waits, int *waits, int threadID) {
    const char* c_kernel_name = kernel_name.c_str();
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_kernel_call(%s, async ID = %d, thread ID = %d)\n",c_kernel_name, async, threadID);
    }
#endif
  HostConf_t *tconf = getHostConf(threadID);
#ifdef _OPENARC_PROFILE_
    double ltime = HI_get_localtime();
#endif
  int err = IRIS_SUCCESS;
  int currentAsync = threadAsyncMap[threadID];
  bool task_exist = false;
  iris_task task;
  if( threadTaskMap.count(threadID) > 0 ) {
  	task = threadTaskMap[threadID];
    task_exist = true;
  }
  int nestingLevel = threadTaskMapNesting[threadID];
  if( !task_exist && (nestingLevel == 0) ) {
    iris_task_create(&task);
  }
  size_t gws[3] = { gridSize[0] * blockSize[0], gridSize[1] * blockSize[1], gridSize[2] * blockSize[2] };
  kernelParams_t *kernelParams = tconf->kernelArgsMap.at(this).at(kernel_name);
  int num_args = kernelParams->num_args;
  for(int i=0; i<num_args; i++) {
    if( kernelParams->kernelParams[i] == NULL ) {
        kernelParams->kernelParams[i] = &(kernelParams->kernelParamMems[i]);
    }
  }
  //iris_task_kernel(task, kernel_name.c_str(), 3, NULL, gws, blockSize, kernelParams->num_args, kernelParams->kernelParams, kernelParams->kernelParamsInfo);
  iris_task_kernel_v2(task, kernel_name.c_str(), 3, NULL, gws, blockSize, kernelParams->num_args, kernelParams->kernelParams, kernelParams->kernelParamsOffset, kernelParams->kernelParamsInfo);

  threadtaskmapiris_t *asyncTaskMap = threadAsyncTaskMap[threadID];
  int nTasks = 0;
  iris_task* dependTaskList = new iris_task[num_waits+1];
  if( (num_waits > 0) && (waits != NULL) ) {
	for(int i=0; i<num_waits; i++) {
		int wait_async = waits[i];
		if( wait_async != async ) {
			if( asyncTaskMap->count(wait_async) > 0 ) {
				dependTaskList[nTasks++] = asyncTaskMap->at(wait_async);
			}	
		}
	}
  }
  if( nestingLevel == 0 ) {
	if( async == DEFAULT_QUEUE+tconf->asyncID_offset ) {
#if IRIS_TASK_SUBMIT_MODE == 0
#ifdef _OPENARC_PROFILE_
    	if( HI_openarcrt_verbosity > 2 ) {
        	fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_kernel_call (%s, async ID = %d, thread ID = %d) submits an IRIS task synchronously to the device %d\n", c_kernel_name, async, threadID, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    	}
#endif
		if( nTasks > 0 ) {
			iris_task_depend(task, nTasks, dependTaskList); 
		}
		err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, true);
		if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
		delete[] dependTaskList;
    	iris_task_release(task);
#else
#ifdef _OPENARC_PROFILE_
    	if( HI_openarcrt_verbosity > 2 ) {
        	fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_kernel_call (%s, async ID = %d, thread ID = %d) submits an IRIS task asynchronously to the device %d\n", c_kernel_name, async, threadID, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    	}
#endif
		if( asyncTaskMap->count(async) > 0 ) {
			dependTaskList[nTasks++] = asyncTaskMap->at(async);
			iris_task_depend(task, nTasks, dependTaskList); 
			err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, false);
			if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
			iris_task_release(asyncTaskMap->at(async));
			delete[] dependTaskList;
		} else {	
			if( nTasks > 0 ) {
				iris_task_depend(task, nTasks, dependTaskList); 
			}

			err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, false);
			if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
			delete[] dependTaskList;
		}
		(*asyncTaskMap)[async] = task;
#endif
	} else {
		if( asyncTaskMap->count(async) > 0 ) {
#ifdef _OPENARC_PROFILE_
    		if( HI_openarcrt_verbosity > 2 ) {
        		fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_kernel_call (%s, async ID = %d, thread ID = %d) submits an IRIS task asynchronously with dependency to the device %d\n", c_kernel_name, async, threadID, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    		}
#endif
			dependTaskList[nTasks++] = asyncTaskMap->at(async);
			iris_task_depend(task, nTasks, dependTaskList); 
			err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, false);
			if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
			iris_task_release(asyncTaskMap->at(async));
			delete[] dependTaskList;
		} else {
#ifdef _OPENARC_PROFILE_
    		if( HI_openarcrt_verbosity > 2 ) {
        		fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_kernel_call (%s, async ID = %d, thread ID = %d) submits an IRIS task asynchronously without dependency to the device %d\n", c_kernel_name, async, threadID, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    		}
#endif
			if( nTasks > 0 ) {
				iris_task_depend(task, nTasks, dependTaskList); 
			}

			err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, false);
			if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
			delete[] dependTaskList;
		}
		(*asyncTaskMap)[async] = task;
	}
#ifdef _OPENARC_PROFILE_
	tconf->BTaskCnt++;	
#endif
  } else {
  	if( (currentAsync == NO_QUEUE) && (nTasks > 0) ) {
  		iris_task_depend(task, nTasks, dependTaskList); 
  	}
  	delete[] dependTaskList;
  	threadAsyncMap[threadID] = async;
  }
#ifdef _OPENARC_PROFILE_
    if(tconf->KernelCNTMap.count(kernel_name) == 0) {
        tconf->KernelCNTMap[kernel_name] = 0;
    }        
    tconf->KernelCNTMap[kernel_name] += 1;
    if(tconf->KernelTimingMap.count(kernel_name) == 0) {
        tconf->KernelTimingMap[kernel_name] = 0.0;
    }        
    tconf->KernelTimingMap[kernel_name] += HI_get_localtime() - ltime;
#endif   

  if (err != IRIS_SUCCESS) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_kernel_call(%s, async ID = %d, thread ID = %d)\n",c_kernel_name, async, threadID);
    }
#endif
  return HI_success;
}

HI_error_t IrisDriver::HI_synchronize( int forcedSync, int threadID ) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_synchronize(forcedSync = %d, thread ID = %d)\n", forcedSync, threadID);
    }
#endif
  int err = IRIS_SUCCESS;
  if( (forcedSync != 0) || (unifiedMemSupported == 1) ) { 
  	HostConf_t * tconf = getHostConf(threadID);
  	threadtaskmapiris_t *asyncTaskMap = threadAsyncTaskMap[threadID];
	int default_async = DEFAULT_QUEUE+tconf->asyncID_offset;
	if( asyncTaskMap->count(default_async) > 0 ) {
#ifdef _OPENARC_PROFILE_
    	if( HI_openarcrt_verbosity > 2 ) {
        	fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_synchronize(thread ID = %d) waits for an IRIS task on async ID = %d\n", threadID, default_async);
    	}    
#endif
		iris_task task = asyncTaskMap->at(default_async);
		iris_task_wait(task);
		iris_task_release(task);
		asyncTaskMap->erase(default_async);
	}
  }
  if (err != IRIS_SUCCESS) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_synchronize(forcedSync = %d, thread ID = %d)\n", forcedSync, threadID);
    }
#endif
  return HI_success;
}

HI_error_t IrisDriver::destroy(int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::destroy()\n");
    }
  	if( HI_openarcrt_verbosity > 4 ) {
  		//fprintf(stderr, "[%s:%d][%s]\n", __FILE__, __LINE__, __func__);
	}
#endif
	NVIDIADeviceIDs.clear();
	AMDDeviceIDs.clear();
	GPUDeviceIDs.clear();
	CPUDeviceIDs.clear();
	FPGADeviceIDs.clear();
	PhiDeviceIDs.clear();
	DefaultDeviceIDs.clear();
	openarcrt_iris_policy = 0;
	openarcrt_iris_dmem = 1;
#ifdef PRINT_TODO
	fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif

#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::destroy()\n");
    }
#endif
  return HI_success;
}

HI_error_t IrisDriver::HI_malloc1D(const void *hostPtr, void **devPtr, size_t count, int asyncID, HI_MallocKind_t flags, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_malloc1D(hostPtr = %lx, asyncID = %d, size = %lu, thread ID = %d)\n",(long unsigned int)hostPtr, asyncID, count, threadID);
    }
#endif

  void* uc_hostPtr = NULL;
  if( openarcrt_iris_dmem == 1 ) {
    uc_hostPtr = const_cast<void*>(hostPtr);
  }

  HostConf_t * tconf = getHostConf(threadID);
  int err;
  iris_mem memHandle;
  if(HI_get_device_address(hostPtr, devPtr, NULL, NULL, asyncID, tconf->threadID) == HI_success ) {
	if( unifiedMemSupported ) {
		err = HI_success;
	} else {
		fprintf(stderr, "[ERROR in IrisDriver::HI_malloc1D()] Duplicate device memory allocation for the same host data (%lx) by thread %d is not allowed; exit!\n",(long unsigned int)hostPtr, tconf->threadID);
		exit(1);
	}    
  } else {
#if VICTIM_CACHE_MODE > 0
    memPool_t *memPool = memPoolMap[tconf->threadID];
    std::multimap<size_t, void *>::iterator it = memPool->find(count);
	if( it != memPool->end()) {
      *devPtr = it->second;
      memPool->erase(it);
      current_mempool_size -= count;
      if( openarcrt_iris_dmem == 1 ) {
        HI_device_mem_handle_t tHandle;
        HI_get_device_mem_handle(*devPtr, &tHandle, tconf->threadID);
		if( openarcrt_iris_dmem == 1 ) {
          iris_data_mem_update((iris_mem) tHandle.memHandle, uc_hostPtr);
        }
      }
      HI_set_device_address(hostPtr, *devPtr, count, asyncID, tconf->threadID);
	} else
#endif
	{
#if VICTIM_CACHE_MODE > 0
		if( current_mempool_size > tconf->max_mempool_size ) {
    		HI_device_mem_handle_t tHandle;
			void * tDevPtr;
			for( it = memPool->begin(); it != memPool->end(); ++it ) {
				tDevPtr = it->second;
    			if( HI_get_device_mem_handle(tDevPtr, &tHandle, tconf->threadID) == HI_success ) { 
					err = iris_mem_release((iris_mem) tHandle.memHandle);
      				if (err != IRIS_SUCCESS) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
      				HI_remove_device_mem_handle(tDevPtr, tconf->threadID);
      				free(tDevPtr);
                    current_mempool_size -= it->first;
#ifdef _OPENARC_PROFILE_
                    tconf->IDFreeCnt++;
                    tconf->CIDMemorySize -= it->first;
#endif
    			}
			}	
			memPool->clear();
		}
#endif	
		
		if( openarcrt_iris_dmem == 1 ) {
			//fprintf(stdout, "[IRIS DMEM CREATE]\n");
			err = iris_data_mem_create((iris_mem*) &memHandle, uc_hostPtr, count);
		} else {
			//fprintf(stdout, "[IRIS MEM CREATE]\n");
      		err = iris_mem_create(count, (iris_mem*) &memHandle);
		}

#if VICTIM_CACHE_MODE > 0
      	if (err != IRIS_SUCCESS) {
    		HI_device_mem_handle_t tHandle;
			void * tDevPtr;
			for( it = memPool->begin(); it != memPool->end(); ++it ) {
				tDevPtr = it->second;
    			if( HI_get_device_mem_handle(tDevPtr, &tHandle, tconf->threadID) == HI_success ) { 
					err = iris_mem_release((iris_mem) tHandle.memHandle);
      				if (err != IRIS_SUCCESS) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
      				HI_remove_device_mem_handle(tDevPtr, tconf->threadID);
      				free(tDevPtr);
                    current_mempool_size -= it->first;
#ifdef _OPENARC_PROFILE_
                    tconf->IDFreeCnt++;
                    tconf->CIDMemorySize -= it->first;
#endif
    			}
			}	
			memPool->clear();
			if( openarcrt_iris_dmem == 1 ) {
				//fprintf(stdout, "[IRIS DMEM CREATE]\n");
				err = iris_data_mem_create((iris_mem*) &memHandle, uc_hostPtr, count);
			} else {
				//fprintf(stdout, "[IRIS MEM CREATE]\n");
      			err = iris_mem_create(count, (iris_mem*) &memHandle);
			}
      		if (err != IRIS_SUCCESS) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
		}
#endif
      	*devPtr = malloc(count);
      	HI_set_device_address(hostPtr, *devPtr, count, asyncID, tconf->threadID);
      	HI_set_device_mem_handle(*devPtr, memHandle, count, tconf->threadID);
#ifdef _OPENARC_PROFILE_
		tconf->IDMallocCnt++;
		tconf->IDMallocSize += count;
		tconf->CIDMemorySize += count;
		if( tconf->MIDMemorySize < tconf->CIDMemorySize ) {
			tconf->MIDMemorySize = tconf->CIDMemorySize;
		}    
#endif
    }
  }
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_malloc1D(hostPtr = %lx, asyncID = %d, size = %lu, thread ID = %d)\n",(long unsigned int)hostPtr, asyncID, count, threadID);
    }
#endif
  return HI_success;
}

HI_error_t IrisDriver::HI_memcpy(void *dst, const void *src, size_t count, HI_MemcpyKind_t kind, int trType, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
		std::string memtrType;
  		switch( kind ) {
    		case HI_MemcpyHostToHost: memtrType = "HostToHost"; break;
    		case HI_MemcpyHostToDevice: memtrType = "HostToDevice"; break;
    		case HI_MemcpyDeviceToHost: memtrType = "DeviceToHost"; break;
    		case HI_MemcpyDeviceToDevice: memtrType = "DeviceToDevice"; break;
		}
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_memcpy(%lu, %s, thread ID = %d)\n", count, memtrType.c_str(), threadID);
    }
#endif
  HostConf_t * tconf = getHostConf(threadID);
#ifdef _OPENARC_PROFILE_
    double ltime = HI_get_localtime();
#endif
  int err = IRIS_SUCCESS;
  switch( kind ) {
    case HI_MemcpyHostToHost:       memcpy(dst, src, count);                        break;
    case HI_MemcpyHostToDevice:
    {
      if( openarcrt_iris_dmem == 1 ) {
        //fprintf(stdout, "NO NEED TO SPECIFY H2D\n");
        break;
      } else {
        HI_device_mem_handle_t tHandle;
        if( HI_get_device_mem_handle(dst, &tHandle, tconf->threadID) == HI_success ) {
  		  threadtaskmapiris_t *asyncTaskMap = threadAsyncTaskMap[threadID];
		  int default_async = DEFAULT_QUEUE+tconf->asyncID_offset;
  		  int currentAsync = threadAsyncMap[threadID];
  		  iris_task task;
          bool task_exist = false;
          if( threadTaskMap.count(threadID) > 0 ) {
  			  task = threadTaskMap[threadID];
              task_exist = true;
          }
  		  int nestingLevel = threadTaskMapNesting[threadID];
  		  if( !task_exist && (nestingLevel == 0) ) {
        	  iris_task_create(&task);
		  }
		  //fprintf(stdout, "[IRIS MEM HOST TO DEVICE]\n");
          iris_task_h2d(task, (iris_mem) tHandle.memHandle, tHandle.offset, count, (void*) src);
  		  if( nestingLevel == 0 ) {
#if IRIS_TASK_SUBMIT_MODE == 0

			err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, true);
			if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
        	iris_task_release(task);
#else
			if( asyncTaskMap->count(default_async) > 0 ) {
#ifdef _OPENARC_PROFILE_
				tconf->BTaskCnt++;	
    			if( HI_openarcrt_verbosity > 2 ) {
        			fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_memcpy(%lu, thread ID = %d) submits an IRIS task with dependency to the device %d\n", count, threadID, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    			}
#endif
				iris_task dependTaskList[1] = { asyncTaskMap->at(default_async) };
				iris_task_depend(task, 1, dependTaskList); 
	
				err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, true);
				if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
				iris_task_release(dependTaskList[0]);
				asyncTaskMap->erase(default_async);
			} else {
#ifdef _OPENARC_PROFILE_
				tconf->BTaskCnt++;	
    			if( HI_openarcrt_verbosity > 2 ) {
        			fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_memcpy(%lu, thread ID = %d) submits an IRIS task without dependency to the device %d\n", count, threadID, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    			}
#endif			
	
				err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, true);
				if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
			}
        	iris_task_release(task);
#endif
		  } else {
  			threadAsyncMap[threadID] = default_async;
		  }
        } else {
		  //[DEBUG] How to handle the error case?
        }
      break;
      }
    }
    case HI_MemcpyDeviceToHost:
    {
		HI_device_mem_handle_t tHandle;
		if( HI_get_device_mem_handle(src, &tHandle, tconf->threadID) == HI_success ) {
  			threadtaskmapiris_t *asyncTaskMap = threadAsyncTaskMap[threadID];
			int default_async = DEFAULT_QUEUE+tconf->asyncID_offset;
  			int currentAsync = threadAsyncMap[threadID];
  			iris_task task;
            bool task_exist = false;
            if( threadTaskMap.count(threadID) > 0 ) {
  			    task = threadTaskMap[threadID];
                task_exist = true;
            }
  			int nestingLevel = threadTaskMapNesting[threadID];
  			if( !task_exist && (nestingLevel == 0) ) {
        		iris_task_create(&task);
			}
            if( openarcrt_iris_dmem == 1 ) {
              iris_mem tMemHandle = tHandle.memHandle;
              if( iris_mem_get_type(tMemHandle) == iris::rt::IRIS_MEM ) {
        	    iris_task_d2h(task, (iris_mem) tMemHandle, tHandle.offset, count, dst);
              } else {
                iris_task_dmem_flush_out(task, (iris_mem) tMemHandle);
              }
            } else {
        	  iris_task_d2h(task, (iris_mem) tHandle.memHandle, tHandle.offset, count, dst);
            }
  			if( nestingLevel == 0 ) {
#if IRIS_TASK_SUBMIT_MODE == 0
	
				err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, true);
				if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
        		iris_task_release(task);
#else
				if( asyncTaskMap->count(default_async) > 0 ) {
#ifdef _OPENARC_PROFILE_
					tconf->BTaskCnt++;	
    				if( HI_openarcrt_verbosity > 2 ) {
        				fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_memcpy(%lu, thread ID = %d) submits an IRIS task with dependency to the device %d\n", count, threadID, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    				}
#endif
					iris_task dependTaskList[1] = { asyncTaskMap->at(default_async) };
					iris_task_depend(task, 1, dependTaskList);
		 
					err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, true);
					if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
					iris_task_release(dependTaskList[0]);
					asyncTaskMap->erase(default_async);
				} else {
#ifdef _OPENARC_PROFILE_
					tconf->BTaskCnt++;	
    				if( HI_openarcrt_verbosity > 2 ) {
        				fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_memcpy(%lu, thread ID = %d) submits an IRIS task without dependency to the device %d\n", count, threadID, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    				}
#endif				
		 
					err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, true);
					if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
				}
        		iris_task_release(task);
#endif
			} else {
  				threadAsyncMap[threadID] = default_async;
			}
      } else {
		//[DEBUG] How to handle the error case?
      }
      break;
    }
    case HI_MemcpyDeviceToDevice:   fprintf(stderr, "[%s:%d][%s] not support D2D\n", __FILE__, __LINE__, __func__); break;
  }
  if (err != IRIS_SUCCESS) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
#ifdef _OPENARC_PROFILE_
    if( dst != src ) {
        if( kind == HI_MemcpyHostToDevice ) {
            tconf->H2DMemTrCnt++;
            tconf->H2DMemTrSize += count;
        } else if( kind == HI_MemcpyDeviceToHost ) {
            tconf->D2HMemTrCnt++;
            tconf->D2HMemTrSize += count;
        } else if( kind == HI_MemcpyDeviceToDevice ) {
            tconf->D2DMemTrCnt++;
            tconf->D2DMemTrSize += count;
        } else {
            tconf->H2HMemTrCnt++;
            tconf->H2HMemTrSize += count;
        }
    }
    tconf->totalMemTrTime += HI_get_localtime() - ltime;
#endif

#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
		std::string memtrType;
  		switch( kind ) {
    		case HI_MemcpyHostToHost: memtrType = "HostToHost"; break;
    		case HI_MemcpyHostToDevice: memtrType = "HostToDevice"; break;
    		case HI_MemcpyDeviceToHost: memtrType = "DeviceToHost"; break;
    		case HI_MemcpyDeviceToDevice: memtrType = "DeviceToDevice"; break;
		}
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_memcpy(%lu, %s, thread ID = %d)\n", count, memtrType.c_str(), threadID);
    }
#endif
  return HI_success;
}

HI_error_t IrisDriver::HI_malloc2D( const void *hostPtr, void** devPtr, size_t* pitch, size_t widthInBytes, size_t height, int asyncID, HI_MallocKind_t flags, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif

  return HI_success;
}

HI_error_t IrisDriver::HI_malloc3D( const void *hostPtr, void** devPtr, size_t* pitch, size_t widthInBytes, size_t height, size_t depth, int asyncID, HI_MallocKind_t flags, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif

  return HI_success;
}

//[DEBUG] Implemented by Seyong; may be incorrect!
HI_error_t IrisDriver::HI_free( const void *hostPtr, int asyncID, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_free(hostPtr = %lx, async ID = %d, thread ID = %d)\n",(long unsigned int)hostPtr, asyncID, threadID);
    }
#endif
  	HostConf_t * tconf = getHostConf(threadID);
#ifdef _OPENARC_PROFILE_
    double ltime = HI_get_localtime();
#endif
  	void *devPtr;
	size_t size;
  	int err;
  	if( (HI_get_device_address(hostPtr, &devPtr, NULL, &size, asyncID, tconf->threadID) != HI_error) ) {
       //If this method is called for unified memory, memory deallocation
       //is skipped; unified memory will be deallocatedd only by 
       //HI_free_unified().
		if( hostPtr != devPtr ) {
#if VICTIM_CACHE_MODE > 0
            //We do not free the device memory; instead put it in the memory pool
            //and remove host-pointer-to-device-pointer mapping
            memPool_t *memPool = memPoolMap[tconf->threadID];
            memPool->insert(std::pair<size_t, void *>(size, devPtr));
            current_mempool_size += size;
            HI_remove_device_address(hostPtr, asyncID, tconf->threadID);
      		if( openarcrt_iris_dmem == 1 ) {
				iris_unregister_pin_memory(const_cast<void*>(hostPtr));
			}
#else
    		HI_device_mem_handle_t tHandle;
    		if( HI_get_device_mem_handle(devPtr, &tHandle, tconf->threadID) == HI_success ) { 
				err = iris_mem_release((iris_mem) tHandle.memHandle);
				if (err != IRIS_SUCCESS) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
            	HI_remove_device_address(hostPtr, asyncID, tconf->threadID);
      			HI_remove_device_mem_handle(devPtr, tconf->threadID);
      			free(devPtr);
#ifdef _OPENARC_PROFILE_
      			tconf->IDFreeCnt++;
      			tconf->CIDMemorySize -= size;
#endif
    		}
#endif
		}
  	}

#ifdef _OPENARC_PROFILE_
    tconf->totalFreeTime += HI_get_localtime() - ltime;
#endif
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_free(hostPtr = %lx, async ID = %d, thread ID = %d)\n",(long unsigned int)hostPtr, asyncID, threadID);
    }
#endif
  return HI_success;
}

HI_error_t IrisDriver::HI_pin_host_memory(const void* hostPtr, size_t size, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif

  return HI_success;
}

void IrisDriver::HI_unpin_host_memory(const void* hostPtr, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif

}

HI_error_t IrisDriver::HI_memcpy_async(void *dst, const void *src, size_t count, HI_MemcpyKind_t kind, int trType, int async, int num_waits, int *waits, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
		std::string memtrType;
  		switch( kind ) {
    		case HI_MemcpyHostToHost: memtrType = "HostToHost"; break;
    		case HI_MemcpyHostToDevice: memtrType = "HostToDevice"; break;
    		case HI_MemcpyDeviceToHost: memtrType = "DeviceToHost"; break;
    		case HI_MemcpyDeviceToDevice: memtrType = "DeviceToDevice"; break;
		}
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_memcpy_async(%lu, %s, async ID = %d, thread ID = %d)\n", count, memtrType.c_str(), async, threadID);
    }
#endif
  HostConf_t * tconf = getHostConf(threadID);
#ifdef _OPENARC_PROFILE_
    double ltime = HI_get_localtime();
#endif
  int err = IRIS_SUCCESS;
  threadtaskmapiris_t *asyncTaskMap = threadAsyncTaskMap[threadID];
  int nTasks = 0;
  iris_task* dependTaskList = new iris_task[num_waits+1];
  int currentAsync = threadAsyncMap[threadID];
  if( (num_waits > 0) && (waits != NULL) ) {
	for(int i=0; i<num_waits; i++) {
		int wait_async = waits[i];
		if( wait_async != async ) {
			if( asyncTaskMap->count(wait_async) > 0 ) {
				dependTaskList[nTasks++] = asyncTaskMap->at(wait_async);
			}	
		}
	}
  }
  switch( kind ) {
    case HI_MemcpyHostToHost:       memcpy(dst, src, count);                        break;
    case HI_MemcpyHostToDevice:
    {
      if( openarcrt_iris_dmem == 1 ) {
        //fprintf(stdout, "NO NEED TO SPECIFY H2D\n");
        break;
      } else {
        HI_device_mem_handle_t tHandle;
        if( HI_get_device_mem_handle(dst, &tHandle, tconf->threadID) == HI_success ) {
  		  iris_task task;
          bool task_exist = false;
          if( threadTaskMap.count(threadID) > 0 ) {
  		    task = threadTaskMap[threadID];
            task_exist = true;
          }
  		  int nestingLevel = threadTaskMapNesting[threadID];
  		  if( !task_exist && (nestingLevel == 0) ) {
        	iris_task_create(&task);
		  }
          iris_task_h2d(task, (iris_mem) tHandle.memHandle, tHandle.offset, count, (void*) src);
  		  if( nestingLevel == 0 ) {
			if( asyncTaskMap->count(async) > 0 ) {
#ifdef _OPENARC_PROFILE_
				tconf->BTaskCnt++;	
    			if( HI_openarcrt_verbosity > 2 ) {
        			fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_memcpy_async(%lu, async ID = %d, thread ID = %d) submits an IRIS task asynchronously with dependency to the device %d\n", count, async, threadID, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    			}
#endif
				dependTaskList[nTasks++] = asyncTaskMap->at(async);
				iris_task_depend(task, nTasks, dependTaskList); 
				err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, false);
				if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
				iris_task_release(asyncTaskMap->at(async));
				delete[] dependTaskList;
			} else {
#ifdef _OPENARC_PROFILE_
				tconf->BTaskCnt++;	
    			if( HI_openarcrt_verbosity > 2 ) {
        			fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_memcpy_async(%lu, async ID = %d, thread ID = %d) submits an IRIS task asynchronously without dependency to the device %d\n", count, async, threadID, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    			}
#endif
				if( nTasks > 0 ) {
					iris_task_depend(task, nTasks, dependTaskList); 
				}
				err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, false);
				if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
				delete[] dependTaskList;
			}
			(*asyncTaskMap)[async] = task;
		  } else {
  			if( (currentAsync == NO_QUEUE) && (nTasks > 0) ) {
  				iris_task_depend(task, nTasks, dependTaskList); 
  			}
  			delete[] dependTaskList;
  			threadAsyncMap[threadID] = async;
		  }
        } else {
		  //[DEBUG] How to handle the error case?
        }
        break;
      }
    }
    case HI_MemcpyDeviceToHost:
    {
		HI_device_mem_handle_t tHandle;
		if( HI_get_device_mem_handle(src, &tHandle, tconf->threadID) == HI_success ) {
  			iris_task task;
            bool task_exist = false;
            if( threadTaskMap.count(threadID) > 0 ) {
  			    task = threadTaskMap[threadID];
                task_exist = true;
            }
  			int nestingLevel = threadTaskMapNesting[threadID];
  			if( !task_exist && (nestingLevel == 0) ) {
        		iris_task_create(&task);
			}
			if( openarcrt_iris_dmem == 1 ) {
            	iris_mem tMemHandle = tHandle.memHandle;
            	if( iris_mem_get_type(tMemHandle) == iris::rt::IRIS_MEM ) {
        	    	iris_task_d2h(task, (iris_mem) tMemHandle, tHandle.offset, count, dst);
            	} else {
                	iris_task_dmem_flush_out(task, (iris_mem) tMemHandle);
            	}
			} else {
        		iris_task_d2h(task, (iris_mem) tHandle.memHandle, tHandle.offset, count, dst);
			}
  			if( nestingLevel == 0 ) {
				if( asyncTaskMap->count(async) > 0 ) {
#ifdef _OPENARC_PROFILE_
					tconf->BTaskCnt++;	
    				if( HI_openarcrt_verbosity > 2 ) {
        				fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_memcpy_async(%lu, async ID = %d, thread ID = %d) submits an IRIS task asynchrnously with dependency to the device %d\n", count, async, threadID, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    				}
#endif
					dependTaskList[nTasks++] = asyncTaskMap->at(async);
					iris_task_depend(task, nTasks, dependTaskList); 
					err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, false);
					if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
					iris_task_release(asyncTaskMap->at(async));
					delete[] dependTaskList;
				} else {
#ifdef _OPENARC_PROFILE_
					tconf->BTaskCnt++;	
    				if( HI_openarcrt_verbosity > 2 ) {
        				fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_memcpy_async(%lu, async ID = %d, thread ID = %d) submits an IRIS task asynchrnously without dependency to the device %d\n", count, async, threadID, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    				}
#endif
					if( nTasks > 0 ) {
						iris_task_depend(task, nTasks, dependTaskList); 
					}
					err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, false);
					if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
					delete[] dependTaskList;
				}
				(*asyncTaskMap)[async] = task;
			} else {
  				if( (currentAsync == NO_QUEUE) && (nTasks > 0) ) {
  					iris_task_depend(task, nTasks, dependTaskList); 
  				}
  				delete[] dependTaskList;
  				threadAsyncMap[threadID] = async;
			}
      } else {
		//[DEBUG] How to handle the error case?
      }
      break;
    }
    case HI_MemcpyDeviceToDevice:   fprintf(stderr, "[%s:%d][%s] not support D2D\n", __FILE__, __LINE__, __func__); break;
  }
  if (err != IRIS_SUCCESS) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
#ifdef _OPENARC_PROFILE_
    if( dst != src ) {
        if( kind == HI_MemcpyHostToDevice ) {
            tconf->H2DMemTrCnt++;
            tconf->H2DMemTrSize += count;
        } else if( kind == HI_MemcpyDeviceToHost ) {
            tconf->D2HMemTrCnt++;
            tconf->D2HMemTrSize += count;
        } else if( kind == HI_MemcpyDeviceToDevice ) {
            tconf->D2DMemTrCnt++;
            tconf->D2DMemTrSize += count;
        } else {
            tconf->H2HMemTrCnt++;
            tconf->H2HMemTrSize += count;
        }
    }
    tconf->totalMemTrTime += HI_get_localtime() - ltime;
#endif

#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
		std::string memtrType;
  		switch( kind ) {
    		case HI_MemcpyHostToHost: memtrType = "HostToHost"; break;
    		case HI_MemcpyHostToDevice: memtrType = "HostToDevice"; break;
    		case HI_MemcpyDeviceToHost: memtrType = "DeviceToHost"; break;
    		case HI_MemcpyDeviceToDevice: memtrType = "DeviceToDevice"; break;
		}
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_memcpy_async(%lu, %s, async ID = %d, thread ID = %d)\n", count, memtrType.c_str(), async, threadID);
    }
#endif
  return HI_success;
}

HI_error_t IrisDriver::HI_memcpy_asyncS(void *dst, const void *src, size_t count, HI_MemcpyKind_t kind, int trType, int async, int num_waits, int *waits, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return HI_success;
}

HI_error_t IrisDriver::HI_memcpy2D(void *dst, size_t dpitch, const void *src, size_t spitch, size_t widthInBytes, size_t height, HI_MemcpyKind_t kind, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return HI_success;
}

HI_error_t IrisDriver::HI_memcpy2D_async(void *dst, size_t dpitch, const void *src, size_t spitch, size_t widthInBytes, size_t height, HI_MemcpyKind_t kind, int async, int num_waits, int *waits, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return HI_success;
}

HI_error_t IrisDriver::HI_memcpy2D_asyncS(void *dst, size_t dpitch, const void *src, size_t spitch, size_t widthInBytes, size_t height, HI_MemcpyKind_t kind, int async, int num_waits, int *waits, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return HI_success;
}

//[DEBUG] the current implementation does not exploit victim cache.
//[DEBUG on April 15, 2021] tempPtr is changed to void * type because tempPtr variable itself can be freed before the pointed data are actually freed.
void IrisDriver::HI_tempFree( void* tempPtr, acc_device_t devType, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_tempFree(tempPtr = %lx, devType = %s, thread ID = %d)\n",  (long unsigned int)(tempPtr), HI_get_device_type_string(devType), threadID);
    }
#endif
  	HostConf_t * tconf = getHostConf(threadID);
#ifdef _OPENARC_PROFILE_
    double ltime = HI_get_localtime();
#endif
#ifdef _THREAD_SAFETY
        pthread_mutex_lock(&mutex_tempMalloc);
#else
#ifdef _OPENMP
    #pragma omp critical (tempMalloc_critical)
#endif
#endif
    {
	if(  devType == acc_device_gpu || devType == acc_device_nvidia || 
    devType == acc_device_radeon || devType == acc_device_xeonphi || 
    devType == acc_device_altera || devType == acc_device_altera_emulator ||
    devType == acc_device_current) {
  		void *devPtr;
		size_t size;
  		int err;
  		if( tempPtr != 0 ) {
			//We do not free the device memory; instead put it in the memory pool 
			memPool_t *memPool = NULL;
			if( openarcrt_iris_dmem == 1 ) {
				memPool = tempMemPoolMap[tconf->threadID];
			} else {
				memPool = memPoolMap[tconf->threadID];
			}
			sizemap_t *tempMallocSize = tempMallocSizeMap[tconf->threadID];
#if VICTIM_CACHE_MODE > 0
			if( tempMallocSize->count((const void *)tempPtr) > 0 ) {
				size_t size = tempMallocSize->at((const void *)tempPtr);
				memPool->insert(std::pair<size_t, void *>(size, tempPtr));
            	current_mempool_size += size;
				tempMallocSize->erase((const void *)tempPtr);
			} else 
#endif
			{
    			HI_device_mem_handle_t tHandle;
    			if( HI_get_device_mem_handle(tempPtr, &tHandle, tconf->threadID) == HI_success ) { 
      				err = iris_mem_release((iris_mem) tHandle.memHandle);
      				if (err != IRIS_SUCCESS) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
      				free(tempPtr);
      				HI_remove_device_mem_handle(tempPtr, tconf->threadID);
#ifdef _OPENARC_PROFILE_
            		tconf->IDFreeCnt++;
#endif
					if( tempMallocSize->count((const void *)tempPtr) > 0 ) {
#ifdef _OPENARC_PROFILE_
						size_t size = tempMallocSize->at((const void *)tempPtr);
						tconf->CIDMemorySize -= size;
#endif
						tempMallocSize->erase((const void *)tempPtr);
					}
    			}
			}
		}
  	} else {
		if( tempPtr != 0 ) {
			free(tempPtr);
#ifdef _OPENARC_PROFILE_
            tconf->IHFreeCnt++;
#endif
		}
	}
    }
#ifdef _THREAD_SAFETY
        pthread_mutex_unlock(&mutex_tempMalloc);
#endif
#ifdef _OPENARC_PROFILE_
    tconf->totalFreeTime += HI_get_localtime() - ltime;
#endif
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_tempFree(tempPtr = %lx, devType = %s, thread ID = %d)\n",  (long unsigned int)(tempPtr), HI_get_device_type_string(devType), threadID);
    }
#endif
	tempPtr = 0;
}

//[DEBUG] the current implementation does not exploit victim cache.
void IrisDriver::HI_tempMalloc1D( void** tempPtr, size_t count, acc_device_t devType, HI_MallocKind_t flags, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_tempMalloc1D()\n");
    }
  	if( HI_openarcrt_verbosity > 4 ) {
  		fprintf(stderr, "[%s:%d][%s]\n", __FILE__, __LINE__, __func__);
	}
#endif
  	HostConf_t * tconf = getHostConf(threadID);
#ifdef _OPENARC_PROFILE_
    double ltime = HI_get_localtime();
#endif
#ifdef _THREAD_SAFETY
        pthread_mutex_lock(&mutex_tempMalloc);
#else
#ifdef _OPENMP
    #pragma omp critical (tempMalloc_critical)
#endif
#endif
    {
	if(  devType == acc_device_gpu || devType == acc_device_nvidia || 
    devType == acc_device_radeon || devType == acc_device_xeonphi || 
    devType == acc_device_altera || devType == acc_device_altera_emulator ||
    devType == acc_device_current) {
        sizemap_t *tempMallocSize = tempMallocSizeMap[tconf->threadID];
#if VICTIM_CACHE_MODE > 0
        memPool_t *memPool = NULL;
		if( openarcrt_iris_dmem == 1 ) {
        	memPool = tempMemPoolMap[tconf->threadID];
		} else {
        	memPool = memPoolMap[tconf->threadID];
		}
        std::multimap<size_t, void *>::iterator it = memPool->find(count);
        if (it != memPool->end()) {
#ifdef _OPENARC_PROFILE_
            if( HI_openarcrt_verbosity > 2 ) {
                fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_tempMalloc1D(%lu) reuses memories in the memPool\n", count);
            }
#endif
            *tempPtr = it->second;
            memPool->erase(it);
            current_mempool_size -= count;
            (*tempMallocSize)[(const void *)*tempPtr] = count;
      		//HI_set_device_address(hostPtr, *tempPtr, count, asyncID, tconf->threadID);
        } else 
#endif
		{
			int err;
			iris_mem memHandle;
#if VICTIM_CACHE_MODE > 0
			if( current_mempool_size > tconf->max_mempool_size ) {
#ifdef _OPENARC_PROFILE_
                if( HI_openarcrt_verbosity > 2 ) {
                    fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_tempMalloc1D(%lu) releases memories in the memPool\n", count);
                }
#endif
    			HI_device_mem_handle_t tHandle;
				void * tDevPtr;
                for (it = memPool->begin(); it != memPool->end(); ++it) {
					tDevPtr = it->second;
    				if( HI_get_device_mem_handle(tDevPtr, &tHandle, tconf->threadID) == HI_success ) { 
      					err = iris_mem_release((iris_mem) tHandle.memHandle);
      					if (err != IRIS_SUCCESS) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
      					HI_remove_device_mem_handle(tDevPtr, tconf->threadID);
      					free(tDevPtr);
                        current_mempool_size -= it->first;
#ifdef _OPENARC_PROFILE_
                    	tconf->IDFreeCnt++;
                    	tconf->CIDMemorySize -= it->first;
#endif
    				}
                }
				memPool->clear();
			}
#endif
			err = iris_mem_create(count, (iris_mem*) &memHandle);
#if VICTIM_CACHE_MODE > 0
			if (err != IRIS_SUCCESS) {
#ifdef _OPENARC_PROFILE_
                if( HI_openarcrt_verbosity > 2 ) {
                    fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_tempMalloc1D(%lu) releases memories in the memPool\n", count);
                }
#endif
    			HI_device_mem_handle_t tHandle;
				void * tDevPtr;
                for (it = memPool->begin(); it != memPool->end(); ++it) {
					tDevPtr = it->second;
    				if( HI_get_device_mem_handle(tDevPtr, &tHandle, tconf->threadID) == HI_success ) { 
      					err = iris_mem_release((iris_mem) tHandle.memHandle);
      					if (err != IRIS_SUCCESS) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
      					HI_remove_device_mem_handle(tDevPtr, tconf->threadID);
      					free(tDevPtr);
                        current_mempool_size -= it->first;
#ifdef _OPENARC_PROFILE_
                    	tconf->IDFreeCnt++;
                    	tconf->CIDMemorySize -= it->first;
#endif
    				}
                }
				memPool->clear();
      			err = iris_mem_create(count, (iris_mem*) &memHandle);
      			if (err != IRIS_SUCCESS) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
			}
#endif
			*tempPtr = malloc(count);
			HI_set_device_mem_handle(*tempPtr, memHandle, count, tconf->threadID);
			//New temporary device memory is allocated.
            (*tempMallocSize)[(const void *)*tempPtr] = count;
#ifdef _OPENARC_PROFILE_
        	tconf->IDMallocCnt++;
        	tconf->IDMallocSize += count;
        	tconf->CIDMemorySize += count;
        	if( tconf->MIDMemorySize < tconf->CIDMemorySize ) {
            	tconf->MIDMemorySize = tconf->CIDMemorySize;
        	}   
#endif
		}
	} else {
		*tempPtr = malloc(count);
#ifdef _OPENARC_PROFILE_
        tconf->IHMallocCnt++;
        tconf->IHMallocSize += count;
        tconf->CIHMemorySize += count;
        if( tconf->MIHMemorySize < tconf->CIHMemorySize ) {
            tconf->MIHMemorySize = tconf->CIHMemorySize;
        }   
#endif
	}
    }
#ifdef _THREAD_SAFETY
        pthread_mutex_unlock(&mutex_tempMalloc);
#endif
#ifdef _OPENARC_PROFILE_
    tconf->totalMallocTime += HI_get_localtime() - ltime;
#endif
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_tempMalloc1D()\n");
    }
#endif
}

//[DEBUG] the current implementation does not exploit victim cache.
void IrisDriver::HI_tempMalloc1D_async( void** tempPtr, size_t count, acc_device_t devType, HI_MallocKind_t flags, int asyncID, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_tempMalloc1D_async()\n");
    }
#endif
  	HostConf_t * tconf = getHostConf(threadID);
#ifdef _OPENARC_PROFILE_
    double ltime = HI_get_localtime();
#endif
#ifdef _THREAD_SAFETY
        pthread_mutex_lock(&mutex_tempMalloc);
#else
#ifdef _OPENMP
    #pragma omp critical (tempMalloc_critical)
#endif
#endif
    {
	if(  devType == acc_device_gpu || devType == acc_device_nvidia || 
    devType == acc_device_radeon || devType == acc_device_xeonphi || 
    devType == acc_device_altera || devType == acc_device_altera_emulator ||
    devType == acc_device_current) {
        sizemap_t *tempMallocSize = tempMallocSizeMap[tconf->threadID];
#if VICTIM_CACHE_MODE > 0
        memPool_t *memPool = NULL;
		if( openarcrt_iris_dmem == 1 ) {
        	memPool = tempMemPoolMap[tconf->threadID];
		} else {
        	memPool = memPoolMap[tconf->threadID];
		}
        std::multimap<size_t, void *>::iterator it = memPool->find(count);
        if (it != memPool->end()) {
#ifdef _OPENARC_PROFILE_
            if( HI_openarcrt_verbosity > 2 ) {
                fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_tempMalloc1D_async(%lu) reuses memories in the memPool\n", count);
            }
#endif
            *tempPtr = it->second;
            memPool->erase(it);
            current_mempool_size -= it->first;
            (*tempMallocSize)[(const void *)*tempPtr] = count;
      		//HI_set_device_address(hostPtr, *tempPtr, count, asyncID, tconf->threadID);
        } else 
#endif
		{
			int err;
			iris_mem memHandle;
#if VICTIM_CACHE_MODE > 0
			if( current_mempool_size > tconf->max_mempool_size ) {
#ifdef _OPENARC_PROFILE_
                if( HI_openarcrt_verbosity > 2 ) {
                    fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_tempMalloc1D_async(%lu) releases memories in the memPool\n", count);
                }
#endif
    			HI_device_mem_handle_t tHandle;
				void * tDevPtr;
                for (it = memPool->begin(); it != memPool->end(); ++it) {
					tDevPtr = it->second;
    				if( HI_get_device_mem_handle(tDevPtr, &tHandle, tconf->threadID) == HI_success ) { 
      					err = iris_mem_release((iris_mem) tHandle.memHandle);
      					if (err != IRIS_SUCCESS) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
      					HI_remove_device_mem_handle(tDevPtr, tconf->threadID);
      					free(tDevPtr);
                        current_mempool_size -= it->first;
#ifdef _OPENARC_PROFILE_
                    	tconf->IDFreeCnt++;
                    	tconf->CIDMemorySize -= it->first;
#endif
    				}
                }
				memPool->clear();
			}
#endif
			err = iris_mem_create(count, (iris_mem*) &memHandle);
#if VICTIM_CACHE_MODE > 0
			if (err != IRIS_SUCCESS) {
#ifdef _OPENARC_PROFILE_
                if( HI_openarcrt_verbosity > 2 ) {
                    fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_tempMalloc1D_async(%lu) releases memories in the memPool\n", count);
                }
#endif
    			HI_device_mem_handle_t tHandle;
				void * tDevPtr;
                for (it = memPool->begin(); it != memPool->end(); ++it) {
					tDevPtr = it->second;
    				if( HI_get_device_mem_handle(tDevPtr, &tHandle, tconf->threadID) == HI_success ) { 
      					err = iris_mem_release((iris_mem) tHandle.memHandle);
      					if (err != IRIS_SUCCESS) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
      					HI_remove_device_mem_handle(tDevPtr, tconf->threadID);
      					free(tDevPtr);
                        current_mempool_size -= it->first;
#ifdef _OPENARC_PROFILE_
                    	tconf->IDFreeCnt++;
                    	tconf->CIDMemorySize -= it->first;
#endif
    				}
                }
				memPool->clear();
      			err = iris_mem_create(count, (iris_mem*) &memHandle);
      			if (err != IRIS_SUCCESS) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
			}
#endif
			*tempPtr = malloc(count);
			HI_set_device_mem_handle(*tempPtr, memHandle, count, tconf->threadID);
			//New temporary device memory is allocated.
            (*tempMallocSize)[(const void *)*tempPtr] = count;
#ifdef _OPENARC_PROFILE_
        	tconf->IDMallocCnt++;
        	tconf->IDMallocSize += count;
			tconf->CIDMemorySize += count;
			if( tconf->MIDMemorySize < tconf->CIDMemorySize ) {
				tconf->MIDMemorySize = tconf->CIDMemorySize;
			}
#endif
		}
	} else {
		*tempPtr = malloc(count);
#ifdef _OPENARC_PROFILE_
        tconf->IHMallocCnt++;
        tconf->IHMallocSize += count;
		tconf->CIHMemorySize += count;
		if( tconf->MIHMemorySize < tconf->CIHMemorySize ) {
			tconf->MIHMemorySize = tconf->CIHMemorySize;
		}
#endif
	}
    }
#ifdef _THREAD_SAFETY
        pthread_mutex_unlock(&mutex_tempMalloc);
#endif
#ifdef _OPENARC_PROFILE_
    tconf->totalMallocTime += HI_get_localtime() - ltime;
#endif
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_tempMalloc1D_async()\n");
    }
#endif
}

// Experimental API to support unified memory //
HI_error_t IrisDriver::HI_malloc1D_unified(const void *hostPtr, void **devPtr, size_t count, int asyncID, HI_MallocKind_t flags, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return HI_success;
}

HI_error_t IrisDriver::HI_memcpy_unified(void *dst, const void *src, size_t count, HI_MemcpyKind_t kind, int trType, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return HI_success;
}

HI_error_t IrisDriver::HI_free_unified( const void *hostPtr, int asyncID, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return HI_success;
}

int IrisDriver::HI_get_num_devices(acc_device_t devType, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_get_num_devices(devType = %s)\n", HI_get_device_type_string(devType));
    }
#endif
  int count = 0;
	switch (devType) {
		case acc_device_default: { count = DefaultDeviceIDs.size(); break; }
		case acc_device_host: { count = CPUDeviceIDs.size(); break; }
		case acc_device_not_host: { count = DefaultDeviceIDs.size(); break; }
		case acc_device_nvidia: { count = NVIDIADeviceIDs.size(); break; }
		case acc_device_radeon: { count = AMDDeviceIDs.size(); break; }
		case acc_device_gpu: { count = GPUDeviceIDs.size(); break; }
		case acc_device_xeonphi: { count = PhiDeviceIDs.size(); break; }
		case acc_device_current: { count = DefaultDeviceIDs.size(); break; }
		case acc_device_altera: { count = FPGADeviceIDs.size(); break; }
		case acc_device_altera_emulator: { count = FPGADeviceIDs.size(); break; }
		default: { count = DefaultDeviceIDs.size(); break; }
	}
#ifdef _OPENARC_PROFILE_
  if( HI_openarcrt_verbosity > 4 ) {
  	fprintf(stderr, "[%s:%d][%s] count[%d]\n", __FILE__, __LINE__, __func__, count);
  }
#endif

#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_get_num_devices(devType = %s)\n", HI_get_device_type_string(devType));
    }
#endif
  return count;
}

int IrisDriver::HI_get_num_devices_init(acc_device_t devType, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_get_num_devices_init(devType = %s)\n", HI_get_device_type_string(devType));
    }
#endif
  int err;
  err = iris_init(NULL, NULL, 1);
  if (err == IRIS_SUCCESS) iris_register_command(0xdeadcafe, iris_nvidia, bind_tex_handler);
  else if (err != IRIS_SUCCESS) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
  int ndevs = 0;
  err = iris_device_count(&ndevs);
  if (err != IRIS_SUCCESS) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
  int defaultType = iris_default;
  int defaultTypeFound = 0;
  for(int i=0; i<ndevs; i++) {
  	int type;
  	size_t size;
    err = iris_device_info(i, iris_type, &type, &size);  
  	if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); }
    else {
	  if( defaultTypeFound==0 ) {
        defaultType = type;
        defaultTypeFound = 1;
      }
      if( type == defaultType ) {
        DefaultDeviceIDs.push_back(i);
      }
      if( type == iris_nvidia ) {
        NVIDIADeviceIDs.push_back(i);
        GPUDeviceIDs.push_back(i);
      } else if( type == iris_amd ) {
        AMDDeviceIDs.push_back(i);
        GPUDeviceIDs.push_back(i);
      } else if( type == iris_gpu ) {
        GPUDeviceIDs.push_back(i);
      } else if( type == iris_cpu ) {
        CPUDeviceIDs.push_back(i);
      } else if( type == iris_fpga ) {
        FPGADeviceIDs.push_back(i);
      } else if( type == iris_phi ) {
        PhiDeviceIDs.push_back(i);
      }
    }
  }
  //Set IRIS policy if existing.
  const char * envVar = getenv(openarcrt_iris_policy_env);
  if( envVar == NULL ) {
	openarcrt_iris_policy = 0;
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity >= 0 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tIRIS Policy: none\n");
    }
#endif
  } else {
	if( strcmp(envVar, iris_policy_roundrobin) == 0 ) {
		openarcrt_iris_policy = iris_roundrobin;
	} else if( strcmp(envVar, iris_policy_depend) == 0 ) {
		openarcrt_iris_policy = iris_depend;
	} else if( strcmp(envVar, iris_policy_data) == 0 ) {
		openarcrt_iris_policy = iris_data;
	} else if( strcmp(envVar, iris_policy_profile) == 0 ) {
		openarcrt_iris_policy = iris_profile;
	} else if( strcmp(envVar, iris_policy_random) == 0 ) {
		openarcrt_iris_policy = iris_random;
	} else if( strcmp(envVar, iris_policy_sdq) == 0 ) {
		openarcrt_iris_policy = iris_sdq;
	} else if( strcmp(envVar, iris_policy_ftf) == 0 ) {
		openarcrt_iris_policy = iris_ftf;
	} else if( strcmp(envVar, iris_policy_custom) == 0 ) {
		openarcrt_iris_policy = iris_custom;
	} else {
		openarcrt_iris_policy = 0;
		envVar = "none";
	}
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity >= 0 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tIRIS Policy: %s\n", envVar);
    }
#endif
  }
  //Set IRIS DMEM setting.
  envVar = getenv(openarcrt_iris_dmem_env);
  if( envVar == NULL ) {
	openarcrt_iris_dmem = 1;
  } else {
    openarcrt_iris_dmem = atoi(envVar);
  }
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity >= 0 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tIRIS DMEM: %d\n", openarcrt_iris_dmem);
    }
#endif

  int count = 1;
  if (err != IRIS_SUCCESS) { count = 0; }
#ifdef _OPENARC_PROFILE_
  if( HI_openarcrt_verbosity > 4 ) {
  	fprintf(stderr, "[%s:%d][%s] count[%d]\n", __FILE__, __LINE__, __func__, count);
  }
#endif

#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_get_num_devices_init(devType = %s)\n", HI_get_device_type_string(devType));
    }
#endif
  return count;
}

void IrisDriver::HI_malloc(void **devPtr, size_t size, HI_MallocKind_t flags, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_malloc()\n");
    }    
#endif
#ifdef _OPENARC_PROFILE_
    double ltime = HI_get_localtime();
#endif
  HostConf_t * tconf = getHostConf(threadID);
  int err;
  iris_mem memHandle;
  //fprintf(stdout, "[IRIS MEM CREATE]\n");
  err = iris_mem_create(size, (iris_mem*) &memHandle);

  if (err != IRIS_SUCCESS) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
  *devPtr = malloc(size);
  HI_set_device_mem_handle(*devPtr, memHandle, size, tconf->threadID);
#ifdef _OPENARC_PROFILE_
    tconf->IDMallocCnt++;
    tconf->IDMallocSize += size;
    tconf->CIDMemorySize += size;
    if( tconf->MIDMemorySize < tconf->CIDMemorySize ) {
        tconf->MIDMemorySize = tconf->CIDMemorySize;
    }    
#endif
#ifdef _OPENARC_PROFILE_
    tconf->totalMallocTime += HI_get_localtime() - ltime;
#endif
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_malloc()\n");
    }    
#endif
}

void IrisDriver::HI_free(void *devPtr, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_free(devPtr = %lx, thread ID = %d)\n", (long unsigned int)devPtr, threadID);
    }    
#endif
#ifdef _OPENARC_PROFILE_
    double ltime = HI_get_localtime();
#endif
  HostConf_t * tconf = getHostConf(threadID);
  void *devPtr2;
  int err;
  size_t memSize = 0;
  if( (HI_get_device_address(devPtr, &devPtr2, NULL, &memSize, DEFAULT_QUEUE+tconf->asyncID_offset, tconf->threadID) == HI_error) || (devPtr != devPtr2) ) {
    HI_device_mem_handle_t tHandle;
    if( HI_get_device_mem_handle(devPtr, &tHandle, tconf->threadID) == HI_success ) { 
      err = iris_mem_release((iris_mem) tHandle.memHandle);
      if (err != IRIS_SUCCESS) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
      free(devPtr);
      HI_remove_device_mem_handle(devPtr, tconf->threadID);
#ifdef _OPENARC_PROFILE_
      tconf->IDFreeCnt++;
      tconf->CIDMemorySize -= memSize;
#endif
    }
  }
#ifdef _OPENARC_PROFILE_
    tconf->totalFreeTime += HI_get_localtime() - ltime;
#endif
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_free(devPtr = %lx, thread ID = %d)\n", (long unsigned int)devPtr, threadID);
    }    
#endif
}

HI_error_t IrisDriver::createKernelArgMap(int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::createKernelArgMap(thread ID = %d)\n", threadID);
    }
#endif
  int err;
  HostConf_t * tconf = getHostConf(threadID);
#ifdef _THREAD_SAFETY
  pthread_mutex_lock(&mutex_clContext);
#else
#ifdef _OPENMP
#pragma omp critical(clContext_critical)
#endif
#endif
  {
  std::map<std::string, iris_kernel> kernelMap;
  std::map<std::string, kernelParams_t*> kernelArgs;
  for(std::set<std::string>::iterator it=kernelNameSet.begin(); it!=kernelNameSet.end(); ++it) {
    iris_kernel kernel;
    const char *kernelName = (*it).c_str();
    err = iris_kernel_create(kernelName, &kernel);
    if (err != IRIS_SUCCESS) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
    kernelMap[*it] = kernel;

    kernelParams_t *kernelParams = new kernelParams_t;
    kernelParams->num_args = 0;
    kernelArgs.insert(std::pair<std::string, kernelParams_t*>(std::string(kernelName), kernelParams));
  }

  tconf->kernelArgsMap[this] = kernelArgs;
  tconf->kernelsMap[this]=kernelMap;
  }
#ifdef _THREAD_SAFETY
  pthread_mutex_unlock(&mutex_clContext);
#endif

	int thread_id = tconf->threadID;
  if (masterHandleTable.count(thread_id) == 0) {
    masterAddressTableMap[thread_id] = new addresstable_t();
    masterHandleTable[thread_id] = new memhandlemap_t();
    postponedFreeTableMap[thread_id] = new asyncfreetable_t();
    postponedTempFreeTableMap[thread_id] = new asynctempfreetable_t();
    postponedTempFreeTableMap2[thread_id] = new asynctempfreetable2_t();
    memPoolMap[thread_id] = new memPool_t();
    tempMemPoolMap[thread_id] = new memPool_t();
    tempMallocSizeMap[thread_id] = new sizemap_t();
  	threadAsyncMap[threadID] = NO_QUEUE;
  	//threadTaskMap[thread_id] = NULL;
  	threadTaskMapNesting[thread_id] = 0;
    threadHostMemFreeMap[thread_id] = new pointerset_t();
  }

#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::createKernelArgMap(thread ID = %d)\n", threadID);
    }
#endif
  return HI_success;
}

HI_error_t IrisDriver::HI_bind_texref(std::string texName,  HI_datatype_t type, const void *devPtr, size_t size, int threadID) {
  int err;
  HostConf_t * tconf = getHostConf(threadID);
  HI_device_mem_handle_t tHandle;
  const char* name = texName.c_str();
  int default_async = DEFAULT_QUEUE+tconf->asyncID_offset;
  int currentAsync = threadAsyncMap[threadID];
  iris_task task;
  bool task_exist = false;
  if( threadTaskMap.count(threadID) > 0 ) {
      task = threadTaskMap[threadID];
      task_exist = true;
  }
  int nestingLevel = threadTaskMapNesting[threadID];
  threadtaskmapiris_t *asyncTaskMap = threadAsyncTaskMap[threadID];
  pointerset_t *tmpHostMemSet = threadHostMemFreeMap[threadID];
  if( HI_get_device_mem_handle(devPtr, &tHandle, tconf->threadID) == HI_success ) {
  	if( !task_exist && (nestingLevel == 0) ) {
    	iris_task_create(&task);
	}
    void* tmp = malloc(size);
    iris_task_h2d(task, tHandle.memHandle, 0, size, tmp);
  	if( nestingLevel == 0 ) {
#if IRIS_TASK_SUBMIT_MODE == 0
		err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, true);
		if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
    	iris_task_release(task);
#else
		if( asyncTaskMap->count(default_async) > 0 ) {
			iris_task dependTaskList[1] = { asyncTaskMap->at(default_async) };
			iris_task_depend(task, 1, dependTaskList); 
			err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, true);
			if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
			iris_task_release(dependTaskList[0]);
			asyncTaskMap->erase(default_async);
		} else {
			err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, true);
			if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
		}
        iris_task_release(task);
#endif
#ifdef _OPENARC_PROFILE_
		tconf->BTaskCnt++;	
    	if( HI_openarcrt_verbosity > 2 ) {
        	fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_bind_texref(%s, thread ID = %d) submits an IRIS task to the device %d\n", name, threadID, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    	}
#endif
    	free(tmp);
		//[DEBUG] Why don't release the task?
    	//iris_task_release(task);
	} else {
		tmpHostMemSet->insert((const void *)tmp);
  		threadAsyncMap[threadID] = default_async;
	}
  }
  //[DEBUG on May 3, 2021] we don't need duplicate execution of the following
/*
  if( HI_get_device_mem_handle(devPtr, &tHandle, tconf->threadID) == HI_success ) {
  }
*/
  iris_mem mHandle = tHandle.memHandle;

  size_t name_len = strlen(name) + 1;
  int tp = (type == HI_int) ? iris_int : (type == HI_float) ? iris_float : -1;
  size_t params_size = sizeof(name_len) + name_len + sizeof(tp) + sizeof(mHandle) + sizeof(size);
  char* params = (char*) malloc(params_size);
  int off = 0;
  memcpy(params + off, &name_len, sizeof(name_len));
  off += sizeof(name_len);
  memcpy(params + off, name, name_len);
  off += name_len;
  memcpy(params + off, &tp, sizeof(tp));
  off += sizeof(tp);
  memcpy(params + off, &mHandle, sizeof(mHandle));
  off += sizeof(mHandle);
  memcpy(params + off, &size, sizeof(size));
  off += sizeof(size);

  //iris_task task;
  if( nestingLevel == 0 ) {
  	iris_task_create(&task);
  }
  iris_task_custom(task, 0xdeadcafe, params, params_size);
  if( nestingLevel == 0 ) {
    //iris_task_submit(task, iris_default, NULL, true);
#if IRIS_TASK_SUBMIT_MODE == 0
	err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, true);
	if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
    iris_task_release(task);
#else
	if( asyncTaskMap->count(default_async) > 0 ) {
		iris_task dependTaskList[1] = { asyncTaskMap->at(default_async) };
		iris_task_depend(task, 1, dependTaskList); 
		err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, true);
		if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
		iris_task_release(dependTaskList[0]);
		asyncTaskMap->erase(default_async);
	} else {
		err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, true);
		if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
	}
    iris_task_release(task);
#endif
#ifdef _OPENARC_PROFILE_
	tconf->BTaskCnt++;	
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_bind_texref(%s, thread ID = %d) submits an IRIS task2 to the device %d\n", name, threadID, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    }
#endif
	//[DEBUG] Why don't release the task?
    //iris_task_release(task);
  } else {
  	threadAsyncMap[threadID] = default_async;
  }

  return HI_success;
}

HI_error_t IrisDriver::HI_create_texobj(void *texObj,  HI_datatype_t type, const void *devPtr, size_t size, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return HI_success;
}

HI_error_t IrisDriver::HI_destroy_texobj(void *texObj,  const void *devPtr, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return HI_success;
}

HI_error_t IrisDriver::HI_memcpy_const(void *hostPtr, std::string constName, HI_MemcpyKind_t kind, size_t count, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return HI_success;
}

HI_error_t IrisDriver::HI_memcpy_const_async(void *hostPtr, std::string constName, HI_MemcpyKind_t kind, size_t count, int async, int num_waits, int *waits, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return HI_success;
}

HI_error_t IrisDriver::HI_present_or_memcpy_const(void *hostPtr, std::string constName, HI_MemcpyKind_t kind, size_t count, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return HI_success;
}

void IrisDriver::HI_set_async(int asyncId, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_set_async(%d)\n", asyncId);
    }    
#endif
	if( unifiedMemSupported == 0 ) {
		//We need explicit synchronization here since HI_synchronize() does not
		//explicitly synchronize if unified memory is not used.
  		HostConf_t * tconf = getHostConf(threadID);
  		threadtaskmapiris_t *asyncTaskMap = threadAsyncTaskMap[threadID];
		int default_async = DEFAULT_QUEUE+tconf->asyncID_offset;
		if( asyncTaskMap->count(default_async) > 0 ) {
#ifdef _OPENARC_PROFILE_
    		if( HI_openarcrt_verbosity > 2 ) {
        		fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_set_async(%d) waits for an IRIS task on async ID = %d\n", asyncId, default_async);
    		}    
#endif
			iris_task task = asyncTaskMap->at(default_async);
			iris_task_wait(task);
			iris_task_release(task);
			asyncTaskMap->erase(default_async);
		}
    }    
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_set_async(%d)\n", asyncId);
    }    
#endif
}

void IrisDriver::HI_set_context(int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_set_context(thread ID = %d)\n", threadID);
    }
#endif
#ifdef PRINT_TODO
  //fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_set_context(thread ID = %d)\n", threadID);
    }
#endif
}

void IrisDriver::HI_wait(int arg, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_wait (arg = %d, thread ID = %d)\n", arg, threadID);
    }
#endif
	int nestingLevel = threadTaskMapNesting[threadID];
	threadtaskmapiris_t *asyncTaskMap = threadAsyncTaskMap[threadID];
	if( nestingLevel != 0 ) {
		fprintf(stderr, "[ERROR in IrisDriver::HI_wait(arg = %d, thread ID = %d)] HI_wait() should not be called inside an IRIS task region (IRIS task region nesting level = %d)\n", arg, threadID, nestingLevel);
		exit(1); 
	}
	HostConf_t * tconf = getHostConf(threadID);
	if( asyncTaskMap->count(arg) > 0 ) {
#ifdef _OPENARC_PROFILE_
    	if( HI_openarcrt_verbosity > 2 ) {
        	fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_wait(arg = %d, thread ID = %d) waits for an IRIS task on async ID = %d\n", arg, threadID, arg);
    	}    
#endif
		iris_task task = asyncTaskMap->at(arg);
		iris_task_wait(task);
		iris_task_release(task);
		asyncTaskMap->erase(arg);
		pointerset_t *tmpHostMemSet = threadHostMemFreeMap[threadID];
		if( !(tmpHostMemSet->empty()) ) {
			for(std::set<const void *>::iterator it=tmpHostMemSet->begin(); it!=tmpHostMemSet->end(); ++it) {
				free((void *)*it);
			}
				tmpHostMemSet->clear();
		}
	} else {
		fprintf(stderr, "[ERROR in IrisDriver::HI_wait(arg = %d, thread ID = %d)] HI_wait() there is no IRIS task to wait for; exit!\n", arg, threadID);
		exit(1); 
	}
	HI_postponed_free(arg, tconf->threadID);
	HI_postponed_tempFree(arg, tconf->acc_device_type_var, tconf->threadID);

#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_wait (arg = %d, thread ID = %d)\n", arg, threadID);
    }
#endif
}

void IrisDriver::HI_wait_ifpresent(int arg, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_wait_ifpresent (arg = %d, thread ID = %d)\n", arg, threadID);
    }
#endif
	int nestingLevel = threadTaskMapNesting[threadID];
	threadtaskmapiris_t *asyncTaskMap = threadAsyncTaskMap[threadID];
	if( nestingLevel != 0 ) {
		fprintf(stderr, "[ERROR in IrisDriver::HI_wait_ifpresent(arg = %d, thread ID = %d)] HI_wait_ifpresent() should not be called inside an IRIS task region (IRIS task region nesting level = %d)\n", arg, threadID, nestingLevel);
		exit(1); 
	}
	HostConf_t * tconf = getHostConf(threadID);
	if( asyncTaskMap->count(arg) > 0 ) {
#ifdef _OPENARC_PROFILE_
    	if( HI_openarcrt_verbosity > 2 ) {
        	fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_wait_ifpresent(arg = %d, thread ID = %d) waits for an IRIS task on async ID = %d\n", arg, threadID, arg);
    	}    
#endif
		iris_task task = asyncTaskMap->at(arg);
		iris_task_wait(task);
		iris_task_release(task);
		asyncTaskMap->erase(arg);
		pointerset_t *tmpHostMemSet = threadHostMemFreeMap[threadID];
		if( !(tmpHostMemSet->empty()) ) {
			for(std::set<const void *>::iterator it=tmpHostMemSet->begin(); it!=tmpHostMemSet->end(); ++it) {
				free((void *)*it);
			}
				tmpHostMemSet->clear();
		}
	}
	HI_postponed_free(arg, tconf->threadID);
	HI_postponed_tempFree(arg, tconf->acc_device_type_var, tconf->threadID);

#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_wait_ifpresent (arg = %d, thread ID = %d)\n", arg, threadID);
    }
#endif
}

void IrisDriver::HI_waitS1(int arg, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
}

void IrisDriver::HI_waitS2(int arg, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
}

void IrisDriver::HI_wait_all(int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_wait_all (thread ID = %d)\n", threadID);
    }
#endif
	int nestingLevel = threadTaskMapNesting[threadID];
	threadtaskmapiris_t *asyncTaskMap = threadAsyncTaskMap[threadID];
	if( nestingLevel != 0 ) {
		fprintf(stderr, "[ERROR in IrisDriver::HI_wait_all(thread ID = %d)] HI_wait_all() should not be called inside an IRIS task region (IRIS task region nesting level = %d)\n", threadID, nestingLevel);
		exit(1); 
	}
	HostConf_t * tconf = getHostConf(threadID);
	for( std::map<int, iris_task>::iterator it=asyncTaskMap->begin(); it!=asyncTaskMap->end(); ++it ) {
		int arg = it->first;
		iris_task task = it->second;
		iris_task_wait(task);
		iris_task_release(task);
		pointerset_t *tmpHostMemSet = threadHostMemFreeMap[threadID];
		if( !(tmpHostMemSet->empty()) ) {
			for(std::set<const void *>::iterator it=tmpHostMemSet->begin(); it!=tmpHostMemSet->end(); ++it) {
				free((void *)*it);
			}
				tmpHostMemSet->clear();
		}
		HI_postponed_free(arg, tconf->threadID);
		HI_postponed_tempFree(arg, tconf->acc_device_type_var, tconf->threadID);
	}
	asyncTaskMap->clear();

#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_wait_all (thread ID = %d)\n", threadID);
    }
#endif
}

//[DEBUG] Below implementation is inefficient.
void IrisDriver::HI_wait_async(int arg, int async, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_wait_async (arg = %d, async ID = %d, thread ID = %d)\n", arg, async, threadID);
    }
#endif
	int nestingLevel = threadTaskMapNesting[threadID];
	threadtaskmapiris_t *asyncTaskMap = threadAsyncTaskMap[threadID];
	if( nestingLevel != 0 ) {
		fprintf(stderr, "[ERROR in IrisDriver::HI_wait_async(arg = %d, async ID = %d, thread ID = %d)] HI_wait_async() should not be called inside an IRIS task region (IRIS task region nesting level = %d)\n", arg, async, threadID, nestingLevel);
		exit(1); 
	}
	HostConf_t * tconf = getHostConf(threadID);
	if( asyncTaskMap->count(arg) > 0 ) {
#ifdef _OPENARC_PROFILE_
    	if( HI_openarcrt_verbosity > 2 ) {
        	fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_wait_async(arg = %d, async ID = %d, thread ID = %d) waits for an IRIS task on async ID = %d\n", arg, async, threadID, arg);
    	}    
#endif
		iris_task task = asyncTaskMap->at(arg);
		iris_task_wait(task);
		iris_task_release(task);
		asyncTaskMap->erase(arg);
		pointerset_t *tmpHostMemSet = threadHostMemFreeMap[threadID];
		if( !(tmpHostMemSet->empty()) ) {
			for(std::set<const void *>::iterator it=tmpHostMemSet->begin(); it!=tmpHostMemSet->end(); ++it) {
				free((void *)*it);
			}
				tmpHostMemSet->clear();
		}
	} else {
		fprintf(stderr, "[ERROR in IrisDriver::HI_wait_async(arg = %d, async ID = %d, thread ID = %d)] HI_wait_async() there is no IRIS task to wait for; exit!\n", arg, async, threadID);
		exit(1); 
	}
	HI_postponed_free(arg, tconf->threadID);
	HI_postponed_tempFree(arg, tconf->acc_device_type_var, tconf->threadID);

	if( asyncTaskMap->count(async) > 0 ) {
#ifdef _OPENARC_PROFILE_
    	if( HI_openarcrt_verbosity > 2 ) {
        	fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_wait_async(arg = %d, async ID = %d, thread ID = %d) waits for an IRIS task on async ID = %d\n", arg, async, threadID, async);
    	}    
#endif
		iris_task task = asyncTaskMap->at(async);
		iris_task_wait(task);
		iris_task_release(task);
		asyncTaskMap->erase(async);
	}

#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_wait_async (arg = %d, async ID = %d, thread ID = %d)\n", arg, async, threadID);
    }
#endif
}

void IrisDriver::HI_wait_async_ifpresent(int arg, int async, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_wait_async_ifpresent (arg = %d, async ID = %d, thread ID = %d)\n", arg, async, threadID);
    }
#endif
	int nestingLevel = threadTaskMapNesting[threadID];
	threadtaskmapiris_t *asyncTaskMap = threadAsyncTaskMap[threadID];
	if( nestingLevel != 0 ) {
		fprintf(stderr, "[ERROR in IrisDriver::HI_wait_async_ifpresent(arg = %d, async ID = %d, thread ID = %d)] HI_wait_async_ifpresent() should not be called inside an IRIS task region (IRIS task region nesting level = %d)\n", arg, async, threadID, nestingLevel);
		exit(1); 
	}
	HostConf_t * tconf = getHostConf(threadID);
	if( asyncTaskMap->count(arg) > 0 ) {
#ifdef _OPENARC_PROFILE_
    	if( HI_openarcrt_verbosity > 2 ) {
        	fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_wait_async_ifpresent(arg = %d, async ID = %d, thread ID = %d) waits for an IRIS task on async ID = %d\n", arg, async, threadID, arg);
    	}    
#endif
		iris_task task = asyncTaskMap->at(arg);
		iris_task_wait(task);
		iris_task_release(task);
		asyncTaskMap->erase(arg);
		pointerset_t *tmpHostMemSet = threadHostMemFreeMap[threadID];
		if( !(tmpHostMemSet->empty()) ) {
			for(std::set<const void *>::iterator it=tmpHostMemSet->begin(); it!=tmpHostMemSet->end(); ++it) {
				free((void *)*it);
			}
				tmpHostMemSet->clear();
		}
	}
	HI_postponed_free(arg, tconf->threadID);
	HI_postponed_tempFree(arg, tconf->acc_device_type_var, tconf->threadID);

	if( asyncTaskMap->count(async) > 0 ) {
#ifdef _OPENARC_PROFILE_
    	if( HI_openarcrt_verbosity > 2 ) {
        	fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_wait_async_ifpresent(arg = %d, async ID = %d, thread ID = %d) waits for an IRIS task on async ID = %d\n", arg, async, threadID, async);
    	}    
#endif
		iris_task task = asyncTaskMap->at(async);
		iris_task_wait(task);
		iris_task_release(task);
		asyncTaskMap->erase(async);
	}

#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_wait_async_ifpresent (arg = %d, async ID = %d, thread ID = %d)\n", arg, async, threadID);
    }
#endif
}

void IrisDriver::HI_wait_all_async(int async, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_wait_all_async (async ID = %d, thread ID = %d)\n",async, threadID);
    }
#endif
	int nestingLevel = threadTaskMapNesting[threadID];
	threadtaskmapiris_t *asyncTaskMap = threadAsyncTaskMap[threadID];
	if( nestingLevel != 0 ) {
		fprintf(stderr, "[ERROR in IrisDriver::HI_wait_all_async(async ID = %d, thread ID = %d)] HI_wait_all_async() should not be called inside an IRIS task region (IRIS task region nesting level = %d)\n", async, threadID, nestingLevel);
		exit(1); 
	}
	HostConf_t * tconf = getHostConf(threadID);
	for( std::map<int, iris_task>::iterator it=asyncTaskMap->begin(); it!=asyncTaskMap->end(); ++it ) {
		int arg = it->first;
		iris_task task = it->second;
		iris_task_wait(task);
		iris_task_release(task);
		pointerset_t *tmpHostMemSet = threadHostMemFreeMap[threadID];
		if( !(tmpHostMemSet->empty()) ) {
			for(std::set<const void *>::iterator it=tmpHostMemSet->begin(); it!=tmpHostMemSet->end(); ++it) {
				free((void *)*it);
			}
				tmpHostMemSet->clear();
		}
		HI_postponed_free(arg, tconf->threadID);
		HI_postponed_tempFree(arg, tconf->acc_device_type_var, tconf->threadID);
	}
	asyncTaskMap->clear();

#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_wait_all_async (async ID = %d, thread ID = %d)\n",async, threadID);
    }
#endif
}

int IrisDriver::HI_async_test(int asyncId, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return 1;
//[FIXME] Below implementation is fake.
/*
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_async_test (asyncID = %d, thread ID = %d)\n", asyncID, threadID);
    }
#endif
	int nestingLevel = threadTaskMapNesting[threadID];
	threadtaskmapiris_t *asyncTaskMap = threadAsyncTaskMap[threadID];
	if( nestingLevel != 0 ) {
		fprintf(stderr, "[ERROR in IrisDriver::HI_async_test(asyncID = %d, thread ID = %d)] HI_async_test() should not be called inside an IRIS task region (IRIS task region nesting level = %d)\n", asyncID, threadID, nestingLevel);
		exit(1); 
	}
	HostConf_t * tconf = getHostConf(threadID);
	if( asyncTaskMap->count(arg) > 0 ) {
		iris_task task = asyncTaskMap->at(arg);
		iris_task_wait(task);
		iris_task_release(task);
		asyncTaskMap->erase(arg);
		pointerset_t *tmpHostMemSet = threadHostMemFreeMap[threadID];
		if( !(tmpHostMemSet->empty()) ) {
			for(std::set<const void *>::iterator it=tmpHostMemSet->begin(); it!=tmpHostMemSet->end(); ++it) {
				free((void *)*it);
			}
				tmpHostMemSet->clear();
		}
	} else {
		fprintf(stderr, "[ERROR in IrisDriver::HI_async_test(asyncID = %d, thread ID = %d)] HI_async_test() there is no IRIS task to wait for; exit!\n", asyncID, threadID);
		exit(1); 
	}
	HI_postponed_free(arg, tconf->threadID);
	HI_postponed_tempFree(arg, tconf->acc_device_type_var, tconf->threadID);

#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_async_test (asyncID = %d, thread ID = %d)\n", asyncID, threadID);
    }
#endif
  return 1;
*/
}

int IrisDriver::HI_async_test_ifpresent(int asyncId, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return 1;
}

int IrisDriver::HI_async_test_all(int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return 1;
}

void IrisDriver::HI_wait_for_events(int async, int num_waits, int* waits, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
}

void IrisDriver::HI_enter_subregion(const char *label, int threadID) {
  	int nestingLevel = threadTaskMapNesting[threadID];
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_enter_subregion (label = %s, nestlevel = %d, thread ID = %d)\n", label, nestingLevel, threadID);
    }
#endif
  	iris_task task;
    if( threadTaskMap.count(threadID) > 0 ) {
  	    task = threadTaskMap[threadID];
    }
	if( nestingLevel == 0 ) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_enter_subregion (label = %s, nestlevel = %d, thread ID = %d) creates an IRIS task\n", label, nestingLevel, threadID);
    }
#endif
  		iris_task_create(&task);
  		threadTaskMap[threadID] = task;
  		threadAsyncMap[threadID] = NO_QUEUE;
	}
	threadTaskMapNesting[threadID] = ++nestingLevel;
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_enter_subregion (label = %s, nestlevel = %d, thread ID = %d)\n", label, nestingLevel, threadID);
    }
#endif
}

void IrisDriver::HI_exit_subregion(const char *label, int threadID) {
  	int nestingLevel = threadTaskMapNesting[threadID];
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter IrisDriver::HI_exit_subregion (label = %s, nestlevel = %d, thread ID = %d)\n", label, nestingLevel, threadID);
    }
#endif
	int err;
  	int currentAsync = threadAsyncMap[threadID];
  	iris_task task;
    bool task_exist = false;
    if( threadTaskMap.count(threadID) > 0 ) {
  	    task = threadTaskMap[threadID];
        task_exist = true;
    }
	threadtaskmapiris_t *asyncTaskMap = threadAsyncTaskMap[threadID];
    pointerset_t *tmpHostMemSet = threadHostMemFreeMap[threadID];
	nestingLevel--;
	if( nestingLevel <= 0 ) {
		if( task_exist ) {
			int memcpy_cmd_option = 0;
#if OPT_MEMCPY_ONLY_POLICY == 2
			int d2hmemcpy_cmds_cnt = 0;
			int h2dmemcpy_cmds_cnt = 0;
			int ncmds = 0;
			iris_task_info(task, iris_ncmds, &ncmds, NULL);
			int *cmds = (int *)malloc(ncmds*sizeof(int));
			iris_task_info(task, iris_cmds, cmds, NULL);
			for(int i=0; i<ncmds; i++) {
				int ctype = cmds[i];
				if( ctype == IRIS_CMD_D2H ) {
					d2hmemcpy_cmds_cnt += 1;
				} else if( ctype == IRIS_CMD_H2D ) {
					h2dmemcpy_cmds_cnt += 1;
				} else {
					break;
				}
			}
			if( d2hmemcpy_cmds_cnt == ncmds ) {
				memcpy_cmd_option = 1;
			} else if( h2dmemcpy_cmds_cnt == ncmds ) {
				memcpy_cmd_option = 2;
			}
#elif OPT_MEMCPY_ONLY_POLICY == 1
			int d2hmemcpy_cmds_cnt = 0;
			int ncmds = 0;
			iris_task_info(task, iris_ncmds, &ncmds, NULL);
			int *cmds = (int *)malloc(ncmds*sizeof(int));
			iris_task_info(task, iris_cmds, cmds, NULL);
			for(int i=0; i<ncmds; i++) {
				int ctype = cmds[i];
				if( ctype == IRIS_CMD_D2H ) {
					d2hmemcpy_cmds_cnt += 1;
				} else {
					break;
				}
			}
			if( d2hmemcpy_cmds_cnt == ncmds ) {
				memcpy_cmd_option = 1;
			}
#endif
  			HostConf_t *tconf = getHostConf(threadID);
			if( currentAsync == DEFAULT_QUEUE+tconf->asyncID_offset ) {
#if IRIS_TASK_SUBMIT_MODE == 0
#ifdef _OPENARC_PROFILE_
    			if( HI_openarcrt_verbosity > 2 ) {
        			fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_exit_subregion (label = %s, nestlevel = %d, thread ID = %d) submits an IRIS task synchronously to the device %d\n", label, nestingLevel, threadID, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option));
    			}
#endif			
				err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option), NULL, true);
				if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
#ifdef _OPENARC_PROFILE_
				tconf->BTaskCnt++;	
#endif
    			iris_task_release(task);
				if( !(tmpHostMemSet->empty()) ) {
					for(std::set<const void *>::iterator it=tmpHostMemSet->begin(); it!=tmpHostMemSet->end(); ++it) {
						free((void *)*it);
					}
					tmpHostMemSet->clear();
				}
  				threadTaskMap.erase(threadID);
  				threadAsyncMap[threadID] = NO_QUEUE;
				threadTaskMapNesting[threadID] = 0;
#else
				//[DEBUG on Sept. 22, 2024] When IRIS DMEM is used, H2D transfers are implicitly handled
				//and thus, H2D commands are not included in the IRIS task.
				if((openarcrt_iris_dmem == 1) || (iris_task_kernel_cmd_only(task) != IRIS_SUCCESS)) {
					if( asyncTaskMap->count(currentAsync) > 0 ) {
#ifdef _OPENARC_PROFILE_
    					if( HI_openarcrt_verbosity > 2 ) {
        					fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_exit_subregion (label = %s, nestlevel = %d, thread ID = %d) submits an IRIS task synchronously (async ID = %d) with dependency to the device %d\n", label, nestingLevel, threadID, currentAsync, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option));
    					}
#endif
						iris_task dependTaskList[1] = { asyncTaskMap->at(currentAsync) };
						iris_task_depend(task, 1, dependTaskList); 
						err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option), NULL, true);
						if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
						iris_task_release(dependTaskList[0]);
						asyncTaskMap->erase(currentAsync);
					} else {
#ifdef _OPENARC_PROFILE_
    					if( HI_openarcrt_verbosity > 2 ) {
        					fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_exit_subregion (label = %s, nestlevel = %d, thread ID = %d) submits an IRIS task synchronously (async ID = %d) without dependency to the device %d\n", label, nestingLevel, threadID, currentAsync, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option));
    					}
#endif					
						err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option), NULL, true);
						if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
					}
#ifdef _OPENARC_PROFILE_
					tconf->BTaskCnt++;	
#endif
    				iris_task_release(task);
					if( !(tmpHostMemSet->empty()) ) {
						for(std::set<const void *>::iterator it=tmpHostMemSet->begin(); it!=tmpHostMemSet->end(); ++it) {
							free((void *)*it);
						}
						tmpHostMemSet->clear();
					}
  					threadTaskMap.erase(threadID);
  					threadAsyncMap[threadID] = NO_QUEUE;
					threadTaskMapNesting[threadID] = 0;
				} else {
					if( asyncTaskMap->count(currentAsync) > 0 ) {
#ifdef _OPENARC_PROFILE_
    					if( HI_openarcrt_verbosity > 2 ) {
        					fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_exit_subregion (label = %s, nestlevel = %d, thread ID = %d) submits an IRIS task asynchronously (async ID = %d) with dependency to the device %d\n", label, nestingLevel, threadID, currentAsync, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option));
    					}
#endif
						iris_task dependTaskList[1] = { asyncTaskMap->at(currentAsync) };
						iris_task_depend(task, 1, dependTaskList); 
						err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option), NULL, false);
						if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
						iris_task_release(dependTaskList[0]);
						//asyncTaskMap->erase(currentAsync);
					} else {
#ifdef _OPENARC_PROFILE_
    					if( HI_openarcrt_verbosity > 2 ) {
        					fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_exit_subregion (label = %s, nestlevel = %d,thread ID = %d) submits an IRIS task asynchronously (async ID = %d) without dependency to the device %d\n", label, nestingLevel, threadID, currentAsync, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option));
    					}
#endif					
						err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option), NULL, false);
						if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
					}
					(*asyncTaskMap)[currentAsync] = task;
  					threadTaskMap.erase(threadID);
  					threadAsyncMap[threadID] = NO_QUEUE;
					threadTaskMapNesting[threadID] = 0;
#ifdef _OPENARC_PROFILE_
					tconf->BTaskCnt++;	
#endif
				}
#endif
			} else if( currentAsync != NO_QUEUE) {
				if( asyncTaskMap->count(currentAsync) > 0 ) {
#ifdef _OPENARC_PROFILE_
    				if( HI_openarcrt_verbosity > 2 ) {
        				fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_exit_subregion (label = %s, nestlevel = %d,thread ID = %d) submits an IRIS task asynchronously (async ID = %d) with dependency to the device %d\n", label, nestingLevel, threadID, currentAsync, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option));
    				}
#endif
					iris_task dependTaskList[1] = { asyncTaskMap->at(currentAsync) };
					iris_task_depend(task, 1, dependTaskList); 
					err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option), NULL, false);
					if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
					iris_task_release(dependTaskList[0]);
				} else {
#ifdef _OPENARC_PROFILE_
    				if( HI_openarcrt_verbosity > 2 ) {
        				fprintf(stderr, "[OPENARCRT-INFO]\t\tIrisDriver::HI_exit_subregion (label = %s, nestlevel = %d,thread ID = %d) submits an IRIS task asynchronously (async ID = %d) without dependency to the device %d\n", label, nestingLevel, threadID, currentAsync, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option));
    				}
#endif				
					err = iris_task_submit(task, HI_getIrisDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option), NULL, false);
					if (err != IRIS_SUCCESS) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); exit(1); }
				}
				(*asyncTaskMap)[currentAsync] = task;
  				threadTaskMap.erase(threadID);
  				threadAsyncMap[threadID] = NO_QUEUE;
				threadTaskMapNesting[threadID] = 0;
#ifdef _OPENARC_PROFILE_
				tconf->BTaskCnt++;	
#endif
			} else {
				threadTaskMapNesting[threadID] = 0;
			}
		} else {
			threadTaskMapNesting[threadID] = 0;
		}
	} else {
		threadTaskMapNesting[threadID] = nestingLevel;
	}
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit IrisDriver::HI_exit_subregion (label = %s, nestlevel = %d, thread ID = %d)\n", label, nestingLevel, threadID);
    }
#endif
}
