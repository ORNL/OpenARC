#include "openacc.h"
#include "openaccrt_ext.h"
#if defined(OPENARCRT_USE_BRISBANE) && OPENARCRT_USE_BRISBANE == 2
#include <iris/rt/DeviceCUDA.h>
#include <iris/rt/LoaderCUDA.h>
#include <iris/rt/Mem.h>
#include <iris/rt/Command.h>
#else
#include <brisbane/rt/DeviceCUDA.h>
#include <brisbane/rt/LoaderCUDA.h>
#include <brisbane/rt/Mem.h>
#include <brisbane/rt/Command.h>
#endif

//Below macro is used to configure task behaviors assigned to the default queue.
//Set BRISBANE_TASK_SUBMIT_MODE = 0 to submit a task synchronously
//                                1 to submit a task asynchronously if the task contains only a kernel
#define BRISBANE_TASK_SUBMIT_MODE 1
//Below macro is used to optimize IRIS tasks with memory transfers only, when users choose to use Brisbane policy.
//Set OPT_MEMCPY_ONLY_POLICY = 2 to apply brisbane_pending policy to IRIS tasks with H2D memory transfers only 
//                               and brisbane_data policy to IRIS tasks with D2H memory transfers only
//    OPT_MEMCPY_ONLY_POLICY = 1 to apply brisbane_data policy to IRIS tasks with D2H memory transfers only 
//    OPT_MEMCPY_ONLY_POLICY = 0 to apply user-specified policy to all IRIS tasks.
#define OPT_MEMCPY_ONLY_POLICY 2

static const char *openarcrt_brisbane_policy_env = "OPENARCRT_BRISBANE_POLICY";
static const char *brisbane_policy_roundrobin = "brisbane_roundrobin";
static const char *brisbane_policy_depend = "brisbane_depend";
static const char *brisbane_policy_data = "brisbane_data";
static const char *brisbane_policy_profile = "brisbane_profile";
static const char *brisbane_policy_random = "brisbane_random";
static const char *brisbane_policy_pending = "brisbane_pending";
static const char *brisbane_policy_any = "brisbane_any";
static const char *brisbane_policy_all = "brisbane_all";
static const char *brisbane_policy_custom = "brisbane_custom";

//Below structures contain Brisbane device IDs for a given device type.
std::vector<int> BrisbaneDriver::NVIDIADeviceIDs;
std::vector<int> BrisbaneDriver::AMDDeviceIDs;
std::vector<int> BrisbaneDriver::GPUDeviceIDs;
std::vector<int> BrisbaneDriver::CPUDeviceIDs;
std::vector<int> BrisbaneDriver::FPGADeviceIDs;
std::vector<int> BrisbaneDriver::PhiDeviceIDs;
std::vector<int> BrisbaneDriver::DefaultDeviceIDs;
int BrisbaneDriver::openarcrt_brisbane_policy;

int BrisbaneDriver::HI_getBrisbaneDeviceID(acc_device_t devType, acc_device_t userInput, int devnum, int memTrOnly) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_getBrisbaneDeviceID(%s, %d)\n", HI_get_device_type_string(devType), devnum);
    }    
#endif
	int brisbaneDeviceType = brisbane_default;
	int brisbaneDeviceID = devnum;
	if( openarcrt_brisbane_policy != 0 ) {
#ifdef _OPENARC_PROFILE_
    	if( HI_openarcrt_verbosity > 2 ) {
        	fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_getBrisbaneDeviceID(%s, %d)\n", HI_get_device_type_string(devType), devnum);
    	}    
#endif
		if( memTrOnly == 2 ) {
			//If an IRIS task contains H2D memory transfers only, brisbane_pending policy is used to minimize unnecessary memory transfers between devices.
			return brisbane_pending;
		} else if( memTrOnly == 1 ) {
			//If an IRIS task contains D2H memory transfers only, brisbane_data policy is used to minimize unnecessary memory transfers between devices.
			return brisbane_data;
		} else {
			return openarcrt_brisbane_policy;
		}
	}
	switch (devType) {
		case acc_device_default: { brisbaneDeviceType = brisbane_default; break; }
		case acc_device_host: { brisbaneDeviceType = brisbane_cpu; break; }
		case acc_device_not_host: { brisbaneDeviceType = brisbane_default; break; }
		case acc_device_nvidia: { brisbaneDeviceType = brisbane_nvidia; break; }
		case acc_device_radeon: { brisbaneDeviceType = brisbane_amd; break; }
		case acc_device_gpu: { if( userInput == acc_device_nvidia ) {brisbaneDeviceType = brisbane_nvidia;} 
								else if( userInput == acc_device_radeon ) {brisbaneDeviceType = brisbane_amd;}
								else {brisbaneDeviceType = brisbane_gpu;}
								break; }
		case acc_device_xeonphi: { brisbaneDeviceType = brisbane_phi; break; }
		case acc_device_current: { brisbaneDeviceType = brisbane_default; break; }
		case acc_device_altera: { brisbaneDeviceType = brisbane_fpga; break; }
		case acc_device_altera_emulator: { brisbaneDeviceType = brisbane_fpga; break; }
		default: { brisbaneDeviceType = brisbane_default; break; }
	}
	switch (brisbaneDeviceType) {
		case brisbane_default: { if( devnum >= DefaultDeviceIDs.size() ) {
      									fprintf(stderr, "[ERROR in BrisbaneDriver::HI_getBrisbaneDeviceID()] device number (%d) is greater than the number of available devices (%lu) of a given type (brisbane_default).\n",devnum, DefaultDeviceIDs.size());
      									exit(1);
									} else {
										brisbaneDeviceID = DefaultDeviceIDs[devnum];
									}
									break; }
		case brisbane_nvidia: { if( devnum >= NVIDIADeviceIDs.size() ) {
      									fprintf(stderr, "[ERROR in BrisbaneDriver::HI_getBrisbaneDeviceID()] device number (%d) is greater than the number of available devices (%lu) of a given type (brisbane_nvidia).\n",devnum, NVIDIADeviceIDs.size());
      									exit(1);
									} else {
										brisbaneDeviceID = NVIDIADeviceIDs[devnum];
									}
									break; }
		case brisbane_amd: { if( devnum >= AMDDeviceIDs.size() ) {
      									fprintf(stderr, "[ERROR in BrisbaneDriver::HI_getBrisbaneDeviceID()] device number (%d) is greater than the number of available devices (%lu) of a given type (brisbane_amd).\n",devnum, AMDDeviceIDs.size());
      									exit(1);
									} else {
										brisbaneDeviceID = AMDDeviceIDs[devnum];
									}
									break; }
		case brisbane_gpu: { if( devnum >= GPUDeviceIDs.size() ) {
      									fprintf(stderr, "[ERROR in BrisbaneDriver::HI_getBrisbaneDeviceID()] device number (%d) is greater than the number of available devices (%lu) of a given type (brisbane_gpu).\n",devnum, GPUDeviceIDs.size());
      									exit(1);
									} else {
										brisbaneDeviceID = GPUDeviceIDs[devnum];
									}
									break; }
		case brisbane_phi: { if( devnum >= PhiDeviceIDs.size() ) {
      									fprintf(stderr, "[ERROR in BrisbaneDriver::HI_getBrisbaneDeviceID()] device number (%d) is greater than the number of available devices (%lu) of a given type (brisbane_phi).\n",devnum, PhiDeviceIDs.size());
      									exit(1);
									} else {
										brisbaneDeviceID = PhiDeviceIDs[devnum];
									}
									break; }
		case brisbane_fpga: { if( devnum >= FPGADeviceIDs.size() ) {
      									fprintf(stderr, "[ERROR in BrisbaneDriver::HI_getBrisbaneDeviceID()] device number (%d) is greater than the number of available devices (%lu) of a given type (brisbane_fpga).\n",devnum, FPGADeviceIDs.size());
      									exit(1);
									} else {
										brisbaneDeviceID = FPGADeviceIDs[devnum];
									}
									break; }
		case brisbane_cpu: { if( devnum >= CPUDeviceIDs.size() ) {
      									fprintf(stderr, "[ERROR in BrisbaneDriver::HI_getBrisbaneDeviceID()] device number (%d) is greater than the number of available devices (%lu) of a given type (brisbane_cpu).\n",devnum, CPUDeviceIDs.size());
      									exit(1);
									} else {
										brisbaneDeviceID = CPUDeviceIDs[devnum];
									}
									break; }
		default: { if( devnum >= DefaultDeviceIDs.size() ) {
      									fprintf(stderr, "[ERROR in BrisbaneDriver::HI_getBrisbaneDeviceID()] device number (%d) is greater than the number of available devices (%lu) of a given type (brisbane_default).\n",devnum, DefaultDeviceIDs.size());
      									exit(1);
									} else {
										brisbaneDeviceID = DefaultDeviceIDs[devnum];
									}
									break; }
	}
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_getBrisbaneDeviceID(%s, %d)\n", HI_get_device_type_string(devType), devnum);
    }    
#endif
	return brisbaneDeviceID;
}

BrisbaneDriver::BrisbaneDriver(acc_device_t devType, int devNum, std::set<std::string>kernelNames, HostConf_t *conf, int numDevices, const char *baseFileName) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::BrisbaneDriver(%s, %d)\n", HI_get_device_type_string(devType), devNum);
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
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::BrisbaneDriver(%s, %d)\n", HI_get_device_type_string(devType), devNum);
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
  brisbane_mem mHandle = *((brisbane_mem*) (params + off));
  off += sizeof(mHandle);
  size_t size = *((size_t*) (params + off));
  //printf("[%s:%d] namelen[%lu] name[%s] type[%d] mHandle[%p] size[%lu]\n", __FILE__, __LINE__, name_len, name, type, mHandle, size);

  CUtexref tex;
  CUresult err;
  brisbane_mem m = (brisbane_mem) mHandle;
  brisbane::rt::DeviceCUDA* dev = (brisbane::rt::DeviceCUDA*) device;
  brisbane::rt::LoaderCUDA* ld = dev->ld();
  brisbane::rt::Mem* mem = (brisbane::rt::Mem*)m->class_obj;
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
  err = ld->cuTexRefSetFormat(tex, type == brisbane_int ? CU_AD_FORMAT_SIGNED_INT32 : CU_AD_FORMAT_FLOAT, 1);
  if (err != CUDA_SUCCESS) printf("[%s:%d] err[%d]\n", __FILE__, __LINE__, err);
  return 0;
}

HI_error_t BrisbaneDriver::init(int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::init()\n");
    }
#endif

  current_mempool_size = 0;
  HostConf_t * tconf = getHostConf(threadID);
  int thread_id = tconf->threadID;
  unifiedMemSupported = 0;

  int err;
  //[DEBUG on May 13, 2021] below initialization is not needed since it will be done in HI_get_num_devices_init()
  //and there is only one Brisbane device when targeting Brisbane backend.
/*
  err = brisbane_init(NULL, NULL, 1);
  if (err == BRISBANE_OK) brisbane_register_command(0xdeadcafe, brisbane_nvidia, bind_tex_handler);
  int ndevs = 0;
  err = brisbane_device_count(&ndevs);
  if (err != BRISBANE_OK) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
  int defaultType = brisbane_default;
  for(int i=0; i<ndevs; i++) {
  	int type;
  	size_t size;
    err = brisbane_device_info(i, brisbane_type, &type, &size);  
  	if (err != BRISBANE_OK) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
    if( i==0 ) {
      defaultType = type;
    }
    if( type == defaultType ) {
      DefaultDeviceIDs.push_back(i);
    }
    if( type == brisbane_nvidia ) {
      NVIDIADeviceIDs.push_back(i);
      GPUDeviceIDs.push_back(i);
    } else if( type == brisbane_amd ) {
      AMDDeviceIDs.push_back(i);
      GPUDeviceIDs.push_back(i);
    } else if( type == brisbane_gpu ) {
      GPUDeviceIDs.push_back(i);
    } else if( type == brisbane_cpu ) {
      CPUDeviceIDs.push_back(i);
    } else if( type == brisbane_fpga ) {
      FPGADeviceIDs.push_back(i);
    } else if( type == brisbane_phi ) {
      PhiDeviceIDs.push_back(i);
    }
  }
  //Set brisbane policy if existing.
  const char * envVar = getenv(openarcrt_brisbane_policy_env);
  if( envVar == NULL ) {
	openarcrt_brisbane_policy = 0;
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity >= 0 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbane Policy: none\n");
    }
#endif
  } else {
	if( strcmp(envVar, brisbane_policy_roundrobin) == 0 ) {
		openarcrt_brisbane_policy = brisbane_roundrobin;
	} else if( strcmp(envVar, brisbane_policy_depend) == 0 ) {
		openarcrt_brisbane_policy = brisbane_depend;
	} else if( strcmp(envVar, brisbane_policy_data) == 0 ) {
		openarcrt_brisbane_policy = brisbane_data;
	} else if( strcmp(envVar, brisbane_policy_profile) == 0 ) {
		openarcrt_brisbane_policy = brisbane_profile;
	} else if( strcmp(envVar, brisbane_policy_random) == 0 ) {
		openarcrt_brisbane_policy = brisbane_random;
	} else if( strcmp(envVar, brisbane_policy_any) == 0 ) {
		openarcrt_brisbane_policy = brisbane_any;
	} else if( strcmp(envVar, brisbane_policy_all) == 0 ) {
		openarcrt_brisbane_policy = brisbane_all;
	} else if( strcmp(envVar, brisbane_policy_custom) == 0 ) {
		openarcrt_brisbane_policy = brisbane_custom;
	} else {
		openarcrt_brisbane_policy = 0;
		envVar = "none";
	}
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity >= 0 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbane Policy: %s\n", envVar);
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
  tempMallocSizeMap[thread_id] = new sizemap_t();
  threadAsyncMap[thread_id] = NO_QUEUE;
  //threadTaskMap[thread_id] = NULL;
  threadAsyncTaskMap[thread_id] = new threadtaskmapbrisbane_t();
  threadTaskMapNesting[thread_id] = 0;
  threadHostMemFreeMap[thread_id] = new pointerset_t();

  createKernelArgMap(thread_id);
  init_done = 1;
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::init()\n");
    }
#endif
  return HI_success;
}

HI_error_t BrisbaneDriver::HI_register_kernels(std::set<std::string>kernelNames, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_register_kernels(thread ID = %d)\n", threadID);
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
      brisbane_kernel kernel;
      const char *kernelName = (*it).c_str();
      err = brisbane_kernel_create(kernelName, &kernel);
      if (err != BRISBANE_OK) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
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
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_register_kernels(thread ID = %d)\n", threadID);
    }
#endif
  return HI_success;
}

HI_error_t BrisbaneDriver::HI_register_kernel_numargs(std::string kernel_name, int num_args, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_register_kernel_numargs(thread ID = %d)\n", threadID);
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
      kernelParams->kernelParamMems = (brisbane_mem*)malloc(sizeof(brisbane_mem) * num_args);
    } else {
      fprintf(stderr, "[ERROR in BrisbaneDriver::HI_register_kernel_numargs(%s, %d)] num_args should be greater than zero.\n",kernel_name.c_str(), num_args);
      exit(1);
    }
  }
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_register_kernel_numargs(thread ID = %d)\n", threadID);
    }
#endif
  return HI_success;
}

HI_error_t BrisbaneDriver::HI_register_kernel_arg(std::string kernel_name, int arg_index, size_t arg_size, void *arg_value, int arg_type, int arg_trait, size_t unitSize, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_register_kernel_arg(thread ID = %d)\n", threadID);
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
    brisbane_mem iMem;
    *(kernelParams->kernelParamMems + arg_index) = iMem;
    //err = brisbane_kernel_setarg((brisbane_kernel)(tconf->kernelsMap.at(this).at(kernel_name)), arg_index, arg_size, arg_value);
  } else {
    HI_device_mem_handle_t tHandle;
    if( HI_get_device_mem_handle(*((void **)arg_value), &tHandle, tconf->threadID) == HI_success ) {
      *(kernelParams->kernelParams + arg_index) = NULL;
      //*(kernelParams->kernelParamsOffset + arg_index) = 0 - tHandle.offset;
      //*(kernelParams->kernelParamsOffset + arg_index) = tHandle.offset/unitSize;
      *(kernelParams->kernelParamsOffset + arg_index) = tHandle.offset;
      if( arg_trait == 0 ) { //read-only
      	*(kernelParams->kernelParamsInfo + arg_index) = brisbane_r;
      } else if( arg_trait == 1 ) { //write-only
      	*(kernelParams->kernelParamsInfo + arg_index) = brisbane_w;
      } else if( arg_trait == 2 ) { //read-write
      	*(kernelParams->kernelParamsInfo + arg_index) = brisbane_rw;
      } else { //unknown or temporary
      	*(kernelParams->kernelParamsInfo + arg_index) = 0;
      }
      *(kernelParams->kernelParamMems + arg_index) = tHandle.memHandle;
      //err = brisbane_kernel_setmem_off((brisbane_kernel)(tconf->kernelsMap.at(this).at(kernel_name)), arg_index, *((brisbane_mem*) &(tHandle.memHandle)), tHandle.offset, brisbane_rw);
      //if (err != BRISBANE_OK) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
    } else {
		fprintf(stderr, "[ERROR in BrisbaneDriver::HI_register_kernel_arg()] Cannot find a device pointer to memory handle mapping; failed to add argument %d to kernel %s (Brisbane Device)\n", arg_index, kernel_name.c_str());
#ifdef _OPENARC_PROFILE_
		HI_print_device_address_mapping_entries(tconf->threadID);
#endif
		exit(1);
	}
  }
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_register_kernel_arg(thread ID = %d)\n", threadID);
    }
#endif
  return HI_success;
}

HI_error_t BrisbaneDriver::HI_kernel_call(std::string kernel_name, size_t gridSize[3], size_t blockSize[3], int async, int num_waits, int *waits, int threadID) {
    const char* c_kernel_name = kernel_name.c_str();
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_kernel_call(%s, async ID = %d, thread ID = %d)\n",c_kernel_name, async, threadID);
    }
#endif
  HostConf_t *tconf = getHostConf(threadID);
#ifdef _OPENARC_PROFILE_
    double ltime = HI_get_localtime();
#endif
  int err;
  int currentAsync = threadAsyncMap[threadID];
  bool task_exist = false;
  brisbane_task task;
  if( threadTaskMap.count(threadID) > 0 ) {
    task = threadTaskMap[threadID];
    task_exist = true;
  }
  int nestingLevel = threadTaskMapNesting[threadID];
  if( !task_exist && (nestingLevel == 0) ) {
    brisbane_task_create(&task);
  }
  size_t gws[3] = { gridSize[0] * blockSize[0], gridSize[1] * blockSize[1], gridSize[2] * blockSize[2] };
  kernelParams_t *kernelParams = tconf->kernelArgsMap.at(this).at(kernel_name);
  int num_args = kernelParams->num_args;
  for(int i=0; i<num_args; i++) {
    if( kernelParams->kernelParams[i] == NULL ) {
        kernelParams->kernelParams[i] = kernelParams->kernelParamMems[i];
    }
  }
  //brisbane_task_kernel(task, kernel_name.c_str(), 3, NULL, gws, blockSize, kernelParams->num_args, kernelParams->kernelParams, kernelParams->kernelParamsInfo);
  brisbane_task_kernel_v2(task, kernel_name.c_str(), 3, NULL, gws, blockSize, kernelParams->num_args, kernelParams->kernelParams, kernelParams->kernelParamsOffset, kernelParams->kernelParamsInfo);

  threadtaskmapbrisbane_t *asyncTaskMap = threadAsyncTaskMap[threadID];
  int nTasks = 0;
  brisbane_task* dependTaskList = new brisbane_task[num_waits+1];
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
#if BRISBANE_TASK_SUBMIT_MODE == 0
#ifdef _OPENARC_PROFILE_
    	if( HI_openarcrt_verbosity > 2 ) {
        	fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_kernel_call (%s, async ID = %d, thread ID = %d) submits a brisbane task synchronously to the device %d\n", c_kernel_name, async, threadID, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    	}
#endif
		if( nTasks > 0 ) {
			brisbane_task_depend(task, nTasks, dependTaskList); 
		}
    	brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, true);
		delete[] dependTaskList;
    	brisbane_task_release(task);
#else
#ifdef _OPENARC_PROFILE_
    	if( HI_openarcrt_verbosity > 2 ) {
        	fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_kernel_call (%s, async ID = %d, thread ID = %d) submits a brisbane task asynchronously to the device %d\n", c_kernel_name, async, threadID, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    	}
#endif
		if( asyncTaskMap->count(async) > 0 ) {
			dependTaskList[nTasks++] = asyncTaskMap->at(async);
			brisbane_task_depend(task, nTasks, dependTaskList); 
    		brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, false);
			brisbane_task_release(asyncTaskMap->at(async));
			delete[] dependTaskList;
		} else {	
			if( nTasks > 0 ) {
				brisbane_task_depend(task, nTasks, dependTaskList); 
			}
    		brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, false);
			delete[] dependTaskList;
		}
		(*asyncTaskMap)[async] = task;
#endif
	} else {
		if( asyncTaskMap->count(async) > 0 ) {
#ifdef _OPENARC_PROFILE_
    		if( HI_openarcrt_verbosity > 2 ) {
        		fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_kernel_call (%s, async ID = %d, thread ID = %d) submits a brisbane task asynchronously with dependency to the device %d\n", c_kernel_name, async, threadID, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    		}
#endif
			dependTaskList[nTasks++] = asyncTaskMap->at(async);
			brisbane_task_depend(task, nTasks, dependTaskList); 
    		brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, false);
			brisbane_task_release(asyncTaskMap->at(async));
			delete[] dependTaskList;
		} else {
#ifdef _OPENARC_PROFILE_
    		if( HI_openarcrt_verbosity > 2 ) {
        		fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_kernel_call (%s, async ID = %d, thread ID = %d) submits a brisbane task asynchronously without dependency to the device %d\n", c_kernel_name, async, threadID, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    		}
#endif
			if( nTasks > 0 ) {
				brisbane_task_depend(task, nTasks, dependTaskList); 
			}
    		brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, false);
			delete[] dependTaskList;
		}
		(*asyncTaskMap)[async] = task;
	}
#ifdef _OPENARC_PROFILE_
	tconf->BTaskCnt++;	
#endif
  } else {
  	if( (currentAsync == NO_QUEUE) && (nTasks > 0) ) {
  		brisbane_task_depend(task, nTasks, dependTaskList); 
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

  if (err != BRISBANE_OK) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_kernel_call(%s, async ID = %d, thread ID = %d)\n",c_kernel_name, async, threadID);
    }
#endif
  return HI_success;
}

HI_error_t BrisbaneDriver::HI_synchronize( int forcedSync, int threadID ) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_synchronize(forcedSync = %d, thread ID = %d)\n", forcedSync, threadID);
    }
#endif
  int err = BRISBANE_OK;
  if( (forcedSync != 0) || (unifiedMemSupported == 1) ) { 
  	HostConf_t * tconf = getHostConf(threadID);
  	threadtaskmapbrisbane_t *asyncTaskMap = threadAsyncTaskMap[threadID];
	int default_async = DEFAULT_QUEUE+tconf->asyncID_offset;
	if( asyncTaskMap->count(default_async) > 0 ) {
#ifdef _OPENARC_PROFILE_
    	if( HI_openarcrt_verbosity > 2 ) {
        	fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_synchronize(thread ID = %d) waits for a brisbane task on async ID = %d\n", threadID, default_async);
    	}    
#endif
		brisbane_task task = asyncTaskMap->at(default_async);
		brisbane_task_wait(task);
		brisbane_task_release(task);
		asyncTaskMap->erase(default_async);
	}
  }
  if (err != BRISBANE_OK) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_synchronize(forcedSync = %d, thread ID = %d)\n", forcedSync, threadID);
    }
#endif
  return HI_success;
}

HI_error_t BrisbaneDriver::destroy(int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::destroy()\n");
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
	openarcrt_brisbane_policy = 0;
#ifdef PRINT_TODO
	fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif

#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::destroy()\n");
    }
#endif
  return HI_success;
}

HI_error_t BrisbaneDriver::HI_malloc1D(const void *hostPtr, void **devPtr, size_t count, int asyncID, HI_MallocKind_t flags, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_malloc1D(hostPtr = %lx, asyncID = %d, size = %lu, thread ID = %d)\n",(long unsigned int)hostPtr, asyncID, count, threadID);
    }
#endif
  HostConf_t * tconf = getHostConf(threadID);
  int err;
  brisbane_mem memHandle;
  if(HI_get_device_address(hostPtr, devPtr, NULL, NULL, asyncID, tconf->threadID) == HI_success ) {
	if( unifiedMemSupported ) {
		err = HI_success;
	} else {
		fprintf(stderr, "[ERROR in BrisbaneDriver::HI_malloc1D()] Duplicate device memory allocation for the same host data (%lx) by thread %d is not allowed; exit!\n",(long unsigned int)hostPtr, tconf->threadID);
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
      				err = brisbane_mem_release((brisbane_mem) tHandle.memHandle);
      				if (err != BRISBANE_OK) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
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
      	err = brisbane_mem_create(count, (brisbane_mem*) &memHandle);
#if VICTIM_CACHE_MODE > 0
      	if (err != BRISBANE_OK) {
    		HI_device_mem_handle_t tHandle;
			void * tDevPtr;
			for( it = memPool->begin(); it != memPool->end(); ++it ) {
				tDevPtr = it->second;
    			if( HI_get_device_mem_handle(tDevPtr, &tHandle, tconf->threadID) == HI_success ) { 
      				err = brisbane_mem_release((brisbane_mem) tHandle.memHandle);
      				if (err != BRISBANE_OK) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
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
      		err = brisbane_mem_create(count, (brisbane_mem*) &memHandle);
      		if (err != BRISBANE_OK) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
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
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_malloc1D(hostPtr = %lx, asyncID = %d, size = %lu, thread ID = %d)\n",(long unsigned int)hostPtr, asyncID, count, threadID);
    }
#endif
  return HI_success;
}

HI_error_t BrisbaneDriver::HI_memcpy(void *dst, const void *src, size_t count, HI_MemcpyKind_t kind, int trType, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
		std::string memtrType;
  		switch( kind ) {
    		case HI_MemcpyHostToHost: memtrType = "HostToHost"; break;
    		case HI_MemcpyHostToDevice: memtrType = "HostToDevice"; break;
    		case HI_MemcpyDeviceToHost: memtrType = "DeviceToHost"; break;
    		case HI_MemcpyDeviceToDevice: memtrType = "DeviceToDevice"; break;
		}
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_memcpy(%lu, %s, thread ID = %d)\n", count, memtrType.c_str(), threadID);
    }
#endif
  HostConf_t * tconf = getHostConf(threadID);
#ifdef _OPENARC_PROFILE_
    double ltime = HI_get_localtime();
#endif
  int err = BRISBANE_OK;
  switch( kind ) {
    case HI_MemcpyHostToHost:       memcpy(dst, src, count);                        break;
    case HI_MemcpyHostToDevice:
    {
      HI_device_mem_handle_t tHandle;
      if( HI_get_device_mem_handle(dst, &tHandle, tconf->threadID) == HI_success ) {
  		threadtaskmapbrisbane_t *asyncTaskMap = threadAsyncTaskMap[threadID];
		int default_async = DEFAULT_QUEUE+tconf->asyncID_offset;
  		int currentAsync = threadAsyncMap[threadID];
  		brisbane_task task;
        bool task_exist = false;
        if( threadTaskMap.count(threadID) > 0 ) {
  		  task = threadTaskMap[threadID];
          task_exist = true;
        }
  		int nestingLevel = threadTaskMapNesting[threadID];
  		if( !task_exist && (nestingLevel == 0) ) {
        	brisbane_task_create(&task);
		}
        brisbane_task_h2d(task, (brisbane_mem) tHandle.memHandle, tHandle.offset, count, (void*) src);
  		if( nestingLevel == 0 ) {
#if BRISBANE_TASK_SUBMIT_MODE == 0
  			brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, true);
        	brisbane_task_release(task);
#else
			if( asyncTaskMap->count(default_async) > 0 ) {
#ifdef _OPENARC_PROFILE_
				tconf->BTaskCnt++;	
    			if( HI_openarcrt_verbosity > 2 ) {
        			fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_memcpy(%lu, thread ID = %d) submits a brisbane task with dependency to the device %d\n", count, threadID, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    			}
#endif
				brisbane_task dependTaskList[1] = { asyncTaskMap->at(default_async) };
				brisbane_task_depend(task, 1, dependTaskList); 
    			brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, true);
				brisbane_task_release(dependTaskList[0]);
				asyncTaskMap->erase(default_async);
			} else {
#ifdef _OPENARC_PROFILE_
				tconf->BTaskCnt++;	
    			if( HI_openarcrt_verbosity > 2 ) {
        			fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_memcpy(%lu, thread ID = %d) submits a brisbane task without dependency to the device %d\n", count, threadID, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    			}
#endif
    			brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, true);
			}
        	brisbane_task_release(task);
#endif
		} else {
  			threadAsyncMap[threadID] = default_async;
		}
      } else {
		//[DEBUG] How to handle the error case?
      }
      break;
    }
    case HI_MemcpyDeviceToHost:
    {
		HI_device_mem_handle_t tHandle;
		if( HI_get_device_mem_handle(src, &tHandle, tconf->threadID) == HI_success ) {
  			threadtaskmapbrisbane_t *asyncTaskMap = threadAsyncTaskMap[threadID];
			int default_async = DEFAULT_QUEUE+tconf->asyncID_offset;
  			int currentAsync = threadAsyncMap[threadID];
  			brisbane_task task;
            bool task_exist = false;
            if( threadTaskMap.count(threadID) > 0 ) {
  			  task = threadTaskMap[threadID];
              task_exist = true;
            }
  			int nestingLevel = threadTaskMapNesting[threadID];
  			if( !task_exist && (nestingLevel == 0) ) {
        		brisbane_task_create(&task);
			}
        	brisbane_task_d2h(task, (brisbane_mem) tHandle.memHandle, tHandle.offset, count, dst);
  			if( nestingLevel == 0 ) {
#if BRISBANE_TASK_SUBMIT_MODE == 0
  				brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, true);
        		brisbane_task_release(task);
#else
				if( asyncTaskMap->count(default_async) > 0 ) {
#ifdef _OPENARC_PROFILE_
					tconf->BTaskCnt++;	
    				if( HI_openarcrt_verbosity > 2 ) {
        				fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_memcpy(%lu, thread ID = %d) submits a brisbane task with dependency to the device %d\n", count, threadID, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    				}
#endif
					brisbane_task dependTaskList[1] = { asyncTaskMap->at(default_async) };
					brisbane_task_depend(task, 1, dependTaskList); 
  					brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, true);
					brisbane_task_release(dependTaskList[0]);
					asyncTaskMap->erase(default_async);
				} else {
#ifdef _OPENARC_PROFILE_
					tconf->BTaskCnt++;	
    				if( HI_openarcrt_verbosity > 2 ) {
        				fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_memcpy(%lu, thread ID = %d) submits a brisbane task without dependency to the device %d\n", count, threadID, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    				}
#endif
    				brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, true);
				}
        		brisbane_task_release(task);
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
  if (err != BRISBANE_OK) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
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
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_memcpy(%lu, %s, thread ID = %d)\n", count, memtrType.c_str(), threadID);
    }
#endif
  return HI_success;
}

HI_error_t BrisbaneDriver::HI_malloc2D( const void *hostPtr, void** devPtr, size_t* pitch, size_t widthInBytes, size_t height, int asyncID, HI_MallocKind_t flags, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif

  return HI_success;
}

HI_error_t BrisbaneDriver::HI_malloc3D( const void *hostPtr, void** devPtr, size_t* pitch, size_t widthInBytes, size_t height, size_t depth, int asyncID, HI_MallocKind_t flags, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif

  return HI_success;
}

//[DEBUG] Implemented by Seyong; may be incorrect!
HI_error_t BrisbaneDriver::HI_free( const void *hostPtr, int asyncID, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_free(hostPtr = %lx, async ID = %d, thread ID = %d)\n",(long unsigned int)hostPtr, asyncID, threadID);
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
#else
    		HI_device_mem_handle_t tHandle;
    		if( HI_get_device_mem_handle(devPtr, &tHandle, tconf->threadID) == HI_success ) { 
      			err = brisbane_mem_release((brisbane_mem) tHandle.memHandle);
      			if (err != BRISBANE_OK) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
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
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_free(hostPtr = %lx, async ID = %d, thread ID = %d)\n",(long unsigned int)hostPtr, asyncID, threadID);
    }
#endif
  return HI_success;
}

HI_error_t BrisbaneDriver::HI_pin_host_memory(const void* hostPtr, size_t size, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif

  return HI_success;
}

void BrisbaneDriver::HI_unpin_host_memory(const void* hostPtr, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif

}

HI_error_t BrisbaneDriver::HI_memcpy_async(void *dst, const void *src, size_t count, HI_MemcpyKind_t kind, int trType, int async, int num_waits, int *waits, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
		std::string memtrType;
  		switch( kind ) {
    		case HI_MemcpyHostToHost: memtrType = "HostToHost"; break;
    		case HI_MemcpyHostToDevice: memtrType = "HostToDevice"; break;
    		case HI_MemcpyDeviceToHost: memtrType = "DeviceToHost"; break;
    		case HI_MemcpyDeviceToDevice: memtrType = "DeviceToDevice"; break;
		}
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_memcpy_async(%lu, %s, async ID = %d, thread ID = %d)\n", count, memtrType.c_str(), async, threadID);
    }
#endif
  HostConf_t * tconf = getHostConf(threadID);
#ifdef _OPENARC_PROFILE_
    double ltime = HI_get_localtime();
#endif
  int err = BRISBANE_OK;
  threadtaskmapbrisbane_t *asyncTaskMap = threadAsyncTaskMap[threadID];
  int nTasks = 0;
  brisbane_task* dependTaskList = new brisbane_task[num_waits+1];
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
      HI_device_mem_handle_t tHandle;
      if( HI_get_device_mem_handle(dst, &tHandle, tconf->threadID) == HI_success ) {
  		brisbane_task task;
        bool task_exist;
        if( threadTaskMap.count(threadID) > 0 ) {
  		  brisbane_task task = threadTaskMap[threadID];
          task_exist = true;
        }
  		int nestingLevel = threadTaskMapNesting[threadID];
  		if( !task_exist && (nestingLevel == 0) ) {
        	brisbane_task_create(&task);
		}
        brisbane_task_h2d(task, (brisbane_mem) tHandle.memHandle, tHandle.offset, count, (void*) src);
  		if( nestingLevel == 0 ) {
			if( asyncTaskMap->count(async) > 0 ) {
#ifdef _OPENARC_PROFILE_
				tconf->BTaskCnt++;	
    			if( HI_openarcrt_verbosity > 2 ) {
        			fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_memcpy_async(%lu, async ID = %d, thread ID = %d) submits a brisbane task asynchronously with dependency to the device %d\n", count, async, threadID, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    			}
#endif
				dependTaskList[nTasks++] = asyncTaskMap->at(async);
				brisbane_task_depend(task, nTasks, dependTaskList); 
    			brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, false);
				brisbane_task_release(asyncTaskMap->at(async));
				delete[] dependTaskList;
			} else {
#ifdef _OPENARC_PROFILE_
				tconf->BTaskCnt++;	
    			if( HI_openarcrt_verbosity > 2 ) {
        			fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_memcpy_async(%lu, async ID = %d, thread ID = %d) submits a brisbane task asynchronously without dependency to the device %d\n", count, async, threadID, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    			}
#endif
				if( nTasks > 0 ) {
					brisbane_task_depend(task, nTasks, dependTaskList); 
				}
    			brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, false);
				delete[] dependTaskList;
			}
			(*asyncTaskMap)[async] = task;
		} else {
  			if( (currentAsync == NO_QUEUE) && (nTasks > 0) ) {
  				brisbane_task_depend(task, nTasks, dependTaskList); 
  			}
  			delete[] dependTaskList;
  			threadAsyncMap[threadID] = async;
		}
      } else {
		//[DEBUG] How to handle the error case?
      }
      break;
    }
    case HI_MemcpyDeviceToHost:
    {
		HI_device_mem_handle_t tHandle;
		if( HI_get_device_mem_handle(src, &tHandle, tconf->threadID) == HI_success ) {
  			brisbane_task task;
            bool task_exist = false;
            if( threadTaskMap.count(threadID) > 0 ) {
  			  brisbane_task task = threadTaskMap[threadID];
              task_exist = true;
            }
  			int nestingLevel = threadTaskMapNesting[threadID];
  			if( !task_exist && (nestingLevel == 0) ) {
        		brisbane_task_create(&task);
			}
        	brisbane_task_d2h(task, (brisbane_mem) tHandle.memHandle, tHandle.offset, count, dst);
  			if( nestingLevel == 0 ) {
				if( asyncTaskMap->count(async) > 0 ) {
#ifdef _OPENARC_PROFILE_
					tconf->BTaskCnt++;	
    				if( HI_openarcrt_verbosity > 2 ) {
        				fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_memcpy_async(%lu, async ID = %d, thread ID = %d) submits a brisbane task asynchrnously with dependency to the device %d\n", count, async, threadID, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    				}
#endif
					dependTaskList[nTasks++] = asyncTaskMap->at(async);
					brisbane_task_depend(task, nTasks, dependTaskList); 
  					brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, false);
					brisbane_task_release(asyncTaskMap->at(async));
					delete[] dependTaskList;
				} else {
#ifdef _OPENARC_PROFILE_
					tconf->BTaskCnt++;	
    				if( HI_openarcrt_verbosity > 2 ) {
        				fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_memcpy_async(%lu, async ID = %d, thread ID = %d) submits a brisbane task asynchrnously without dependency to the device %d\n", count, async, threadID, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    				}
#endif
					if( nTasks > 0 ) {
						brisbane_task_depend(task, nTasks, dependTaskList); 
					}
    				brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, false);
					delete[] dependTaskList;
				}
				(*asyncTaskMap)[async] = task;
			} else {
  				if( (currentAsync == NO_QUEUE) && (nTasks > 0) ) {
  					brisbane_task_depend(task, nTasks, dependTaskList); 
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
  if (err != BRISBANE_OK) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
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
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_memcpy_async(%lu, %s, async ID = %d, thread ID = %d)\n", count, memtrType.c_str(), async, threadID);
    }
#endif
  return HI_success;
}

HI_error_t BrisbaneDriver::HI_memcpy_asyncS(void *dst, const void *src, size_t count, HI_MemcpyKind_t kind, int trType, int async, int num_waits, int *waits, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return HI_success;
}

HI_error_t BrisbaneDriver::HI_memcpy2D(void *dst, size_t dpitch, const void *src, size_t spitch, size_t widthInBytes, size_t height, HI_MemcpyKind_t kind, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return HI_success;
}

HI_error_t BrisbaneDriver::HI_memcpy2D_async(void *dst, size_t dpitch, const void *src, size_t spitch, size_t widthInBytes, size_t height, HI_MemcpyKind_t kind, int async, int num_waits, int *waits, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return HI_success;
}

HI_error_t BrisbaneDriver::HI_memcpy2D_asyncS(void *dst, size_t dpitch, const void *src, size_t spitch, size_t widthInBytes, size_t height, HI_MemcpyKind_t kind, int async, int num_waits, int *waits, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return HI_success;
}

//[DEBUG] the current implementation does not exploit victim cache.
//[DEBUG on April 15, 2021] tempPtr is changed to void * type because tempPtr variable itself can be freed before the pointed data are actually freed.
void BrisbaneDriver::HI_tempFree( void* tempPtr, acc_device_t devType, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_tempFree(tempPtr = %lx, devType = %s, thread ID = %d)\n",  (long unsigned int)(tempPtr), HI_get_device_type_string(devType), threadID);
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
			memPool_t *memPool = memPoolMap[tconf->threadID];
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
      				err = brisbane_mem_release((brisbane_mem) tHandle.memHandle);
      				if (err != BRISBANE_OK) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
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
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_tempFree(tempPtr = %lx, devType = %s, thread ID = %d)\n",  (long unsigned int)(tempPtr), HI_get_device_type_string(devType), threadID);
    }
#endif
	tempPtr = 0;
}

//[DEBUG] the current implementation does not exploit victim cache.
void BrisbaneDriver::HI_tempMalloc1D( void** tempPtr, size_t count, acc_device_t devType, HI_MallocKind_t flags, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_tempMalloc1D()\n");
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
        memPool_t *memPool = memPoolMap[tconf->threadID];
        std::multimap<size_t, void *>::iterator it = memPool->find(count);
        if (it != memPool->end()) {
#ifdef _OPENARC_PROFILE_
            if( HI_openarcrt_verbosity > 2 ) {
                fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_tempMalloc1D(%lu) reuses memories in the memPool\n", count);
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
			brisbane_mem memHandle;
#if VICTIM_CACHE_MODE > 0
			if( current_mempool_size > tconf->max_mempool_size ) {
#ifdef _OPENARC_PROFILE_
                if( HI_openarcrt_verbosity > 2 ) {
                    fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_tempMalloc1D(%lu) releases memories in the memPool\n", count);
                }
#endif
    			HI_device_mem_handle_t tHandle;
				void * tDevPtr;
                for (it = memPool->begin(); it != memPool->end(); ++it) {
					tDevPtr = it->second;
    				if( HI_get_device_mem_handle(tDevPtr, &tHandle, tconf->threadID) == HI_success ) { 
      					err = brisbane_mem_release((brisbane_mem) tHandle.memHandle);
      					if (err != BRISBANE_OK) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
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
			err = brisbane_mem_create(count, (brisbane_mem*) &memHandle);
#if VICTIM_CACHE_MODE > 0
			if (err != BRISBANE_OK) {
#ifdef _OPENARC_PROFILE_
                if( HI_openarcrt_verbosity > 2 ) {
                    fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_tempMalloc1D(%lu) releases memories in the memPool\n", count);
                }
#endif
    			HI_device_mem_handle_t tHandle;
				void * tDevPtr;
                for (it = memPool->begin(); it != memPool->end(); ++it) {
					tDevPtr = it->second;
    				if( HI_get_device_mem_handle(tDevPtr, &tHandle, tconf->threadID) == HI_success ) { 
      					err = brisbane_mem_release((brisbane_mem) tHandle.memHandle);
      					if (err != BRISBANE_OK) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
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
      			err = brisbane_mem_create(count, (brisbane_mem*) &memHandle);
      			if (err != BRISBANE_OK) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
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
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_tempMalloc1D()\n");
    }
#endif
}

//[DEBUG] the current implementation does not exploit victim cache.
void BrisbaneDriver::HI_tempMalloc1D_async( void** tempPtr, size_t count, acc_device_t devType, HI_MallocKind_t flags, int asyncID, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_tempMalloc1D_async()\n");
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
        memPool_t *memPool = memPoolMap[tconf->threadID];
        std::multimap<size_t, void *>::iterator it = memPool->find(count);
        if (it != memPool->end()) {
#ifdef _OPENARC_PROFILE_
            if( HI_openarcrt_verbosity > 2 ) {
                fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_tempMalloc1D_async(%lu) reuses memories in the memPool\n", count);
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
			brisbane_mem  memHandle;
#if VICTIM_CACHE_MODE > 0
			if( current_mempool_size > tconf->max_mempool_size ) {
#ifdef _OPENARC_PROFILE_
                if( HI_openarcrt_verbosity > 2 ) {
                    fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_malloc1D_async(%lu) releases memories in the memPool\n", count);
                }
#endif
    			HI_device_mem_handle_t tHandle;
				void * tDevPtr;
                for (it = memPool->begin(); it != memPool->end(); ++it) {
					tDevPtr = it->second;
    				if( HI_get_device_mem_handle(tDevPtr, &tHandle, tconf->threadID) == HI_success ) { 
      					err = brisbane_mem_release((brisbane_mem) tHandle.memHandle);
      					if (err != BRISBANE_OK) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
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
			err = brisbane_mem_create(count, (brisbane_mem*) &memHandle);
#if VICTIM_CACHE_MODE > 0
			if (err != BRISBANE_OK) {
#ifdef _OPENARC_PROFILE_
                if( HI_openarcrt_verbosity > 2 ) {
                    fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_malloc1D_async(%lu) releases memories in the memPool\n", count);
                }
#endif
    			HI_device_mem_handle_t tHandle;
				void * tDevPtr;
                for (it = memPool->begin(); it != memPool->end(); ++it) {
					tDevPtr = it->second;
    				if( HI_get_device_mem_handle(tDevPtr, &tHandle, tconf->threadID) == HI_success ) { 
      					err = brisbane_mem_release((brisbane_mem) tHandle.memHandle);
      					if (err != BRISBANE_OK) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
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
      			err = brisbane_mem_create(count, (brisbane_mem*) &memHandle);
      			if (err != BRISBANE_OK) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
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
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_tempMalloc1D_async()\n");
    }
#endif
}

// Experimental API to support unified memory //
HI_error_t BrisbaneDriver::HI_malloc1D_unified(const void *hostPtr, void **devPtr, size_t count, int asyncID, HI_MallocKind_t flags, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return HI_success;
}

HI_error_t BrisbaneDriver::HI_memcpy_unified(void *dst, const void *src, size_t count, HI_MemcpyKind_t kind, int trType, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return HI_success;
}

HI_error_t BrisbaneDriver::HI_free_unified( const void *hostPtr, int asyncID, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return HI_success;
}

int BrisbaneDriver::HI_get_num_devices(acc_device_t devType, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_get_num_devices(devType = %s)\n", HI_get_device_type_string(devType));
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
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_get_num_devices(devType = %s)\n", HI_get_device_type_string(devType));
    }
#endif
  return count;
}

int BrisbaneDriver::HI_get_num_devices_init(acc_device_t devType, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_get_num_devices_init(devType = %s)\n", HI_get_device_type_string(devType));
    }
#endif
  int err;
  err = brisbane_init(NULL, NULL, 1);
  if (err == BRISBANE_OK) brisbane_register_command(0xdeadcafe, brisbane_nvidia, bind_tex_handler);
  else if (err != BRISBANE_OK) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
  int ndevs = 0;
  err = brisbane_device_count(&ndevs);
  if (err != BRISBANE_OK) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
  int defaultType = brisbane_default;
  int defaultTypeFound = 0;
  for(int i=0; i<ndevs; i++) {
  	int type;
  	size_t size;
    err = brisbane_device_info(i, brisbane_type, &type, &size);  
  	if (err != BRISBANE_OK) { fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err); }
    else {
	  if( defaultTypeFound==0 ) {
        defaultType = type;
        defaultTypeFound = 1;
      }
      if( type == defaultType ) {
        DefaultDeviceIDs.push_back(i);
      }
      if( type == brisbane_nvidia ) {
        NVIDIADeviceIDs.push_back(i);
        GPUDeviceIDs.push_back(i);
      } else if( type == brisbane_amd ) {
        AMDDeviceIDs.push_back(i);
        GPUDeviceIDs.push_back(i);
      } else if( type == brisbane_gpu ) {
        GPUDeviceIDs.push_back(i);
      } else if( type == brisbane_cpu ) {
        CPUDeviceIDs.push_back(i);
      } else if( type == brisbane_fpga ) {
        FPGADeviceIDs.push_back(i);
      } else if( type == brisbane_phi ) {
        PhiDeviceIDs.push_back(i);
      }
    }
  }
  //Set brisbane policy if existing.
  const char * envVar = getenv(openarcrt_brisbane_policy_env);
  if( envVar == NULL ) {
	openarcrt_brisbane_policy = 0;
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity >= 0 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbane Policy: none\n");
    }
#endif
  } else {
	if( strcmp(envVar, brisbane_policy_roundrobin) == 0 ) {
		openarcrt_brisbane_policy = brisbane_roundrobin;
	} else if( strcmp(envVar, brisbane_policy_depend) == 0 ) {
		openarcrt_brisbane_policy = brisbane_depend;
	} else if( strcmp(envVar, brisbane_policy_data) == 0 ) {
		openarcrt_brisbane_policy = brisbane_data;
	} else if( strcmp(envVar, brisbane_policy_profile) == 0 ) {
		openarcrt_brisbane_policy = brisbane_profile;
	} else if( strcmp(envVar, brisbane_policy_random) == 0 ) {
		openarcrt_brisbane_policy = brisbane_random;
	} else if( strcmp(envVar, brisbane_policy_any) == 0 ) {
		openarcrt_brisbane_policy = brisbane_any;
	} else if( strcmp(envVar, brisbane_policy_all) == 0 ) {
		openarcrt_brisbane_policy = brisbane_all;
	} else if( strcmp(envVar, brisbane_policy_custom) == 0 ) {
		openarcrt_brisbane_policy = brisbane_custom;
	} else {
		openarcrt_brisbane_policy = 0;
		envVar = "none";
	}
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity >= 0 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbane Policy: %s\n", envVar);
    }
#endif
  }

  int count = 1;
  if (err != BRISBANE_OK) { count = 0; }
#ifdef _OPENARC_PROFILE_
  if( HI_openarcrt_verbosity > 4 ) {
  	fprintf(stderr, "[%s:%d][%s] count[%d]\n", __FILE__, __LINE__, __func__, count);
  }
#endif

#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_get_num_devices_init(devType = %s)\n", HI_get_device_type_string(devType));
    }
#endif
  return count;
}

void BrisbaneDriver::HI_malloc(void **devPtr, size_t size, HI_MallocKind_t flags, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_malloc()\n");
    }    
#endif
#ifdef _OPENARC_PROFILE_
    double ltime = HI_get_localtime();
#endif
  HostConf_t * tconf = getHostConf(threadID);
  int err;
  brisbane_mem memHandle;
  err = brisbane_mem_create(size, (brisbane_mem*) &memHandle);
  if (err != BRISBANE_OK) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
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
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_malloc()\n");
    }    
#endif
}

void BrisbaneDriver::HI_free(void *devPtr, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_free(devPtr = %lx, thread ID = %d)\n", (long unsigned int)devPtr, threadID);
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
      err = brisbane_mem_release((brisbane_mem) tHandle.memHandle);
      if (err != BRISBANE_OK) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
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
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_free(devPtr = %lx, thread ID = %d)\n", (long unsigned int)devPtr, threadID);
    }    
#endif
}

HI_error_t BrisbaneDriver::createKernelArgMap(int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::createKernelArgMap(thread ID = %d)\n", threadID);
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
  std::map<std::string, brisbane_kernel> kernelMap;
  std::map<std::string, kernelParams_t*> kernelArgs;
  for(std::set<std::string>::iterator it=kernelNameSet.begin(); it!=kernelNameSet.end(); ++it) {
    brisbane_kernel kernel;
    const char *kernelName = (*it).c_str();
    err = brisbane_kernel_create(kernelName, &kernel);
    if (err != BRISBANE_OK) fprintf(stderr, "[%s:%d][%s] error[%d]\n", __FILE__, __LINE__, __func__, err);
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
    tempMallocSizeMap[thread_id] = new sizemap_t();
  	threadAsyncMap[threadID] = NO_QUEUE;
  	//threadTaskMap[thread_id] = NULL;
  	threadTaskMapNesting[thread_id] = 0;
    threadHostMemFreeMap[thread_id] = new pointerset_t();
  }

#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::createKernelArgMap(thread ID = %d)\n", threadID);
    }
#endif
  return HI_success;
}

HI_error_t BrisbaneDriver::HI_bind_texref(std::string texName,  HI_datatype_t type, const void *devPtr, size_t size, int threadID) {
  HostConf_t * tconf = getHostConf(threadID);
  HI_device_mem_handle_t tHandle;
  const char* name = texName.c_str();
  int default_async = DEFAULT_QUEUE+tconf->asyncID_offset;
  int currentAsync = threadAsyncMap[threadID];
  brisbane_task task;
  bool task_exist = false;
  if( threadTaskMap.count(threadID) > 0 ) {
    brisbane_task task = threadTaskMap[threadID];
    task_exist = true;
  }
  int nestingLevel = threadTaskMapNesting[threadID];
  threadtaskmapbrisbane_t *asyncTaskMap = threadAsyncTaskMap[threadID];
  pointerset_t *tmpHostMemSet = threadHostMemFreeMap[threadID];
  if( HI_get_device_mem_handle(devPtr, &tHandle, tconf->threadID) == HI_success ) {
  	if( !task_exist && (nestingLevel == 0) ) {
    	brisbane_task_create(&task);
	}
    void* tmp = malloc(size);
    brisbane_task_h2d(task, tHandle.memHandle, 0, size, tmp);
  	if( nestingLevel == 0 ) {
#if BRISBANE_TASK_SUBMIT_MODE == 0
    	brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, true);
    	brisbane_task_release(task);
#else
		if( asyncTaskMap->count(default_async) > 0 ) {
			brisbane_task dependTaskList[1] = { asyncTaskMap->at(default_async) };
			brisbane_task_depend(task, 1, dependTaskList); 
    		brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, true);
			brisbane_task_release(dependTaskList[0]);
			asyncTaskMap->erase(default_async);
		} else {
    		brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, true);
		}
        brisbane_task_release(task);
#endif
#ifdef _OPENARC_PROFILE_
		tconf->BTaskCnt++;	
    	if( HI_openarcrt_verbosity > 2 ) {
        	fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_bind_texref(%s, thread ID = %d) submits a brisbane task to the device %d\n", name, threadID, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    	}
#endif
    	free(tmp);
		//[DEBUG] Why don't release the task?
    	//brisbane_task_release(task);
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
  brisbane_mem mHandle = tHandle.memHandle;

  size_t name_len = strlen(name) + 1;
  int tp = (type == HI_int) ? brisbane_int : (type == HI_float) ? brisbane_float : -1;
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

  //brisbane_task task;
  if( nestingLevel == 0 ) {
  	brisbane_task_create(&task);
  }
  brisbane_task_custom(task, 0xdeadcafe, params, params_size);
  if( nestingLevel == 0 ) {
    //brisbane_task_submit(task, brisbane_default, NULL, true);
#if BRISBANE_TASK_SUBMIT_MODE == 0
    brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, true);
    brisbane_task_release(task);
#else
	if( asyncTaskMap->count(default_async) > 0 ) {
		brisbane_task dependTaskList[1] = { asyncTaskMap->at(default_async) };
		brisbane_task_depend(task, 1, dependTaskList); 
    	brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, true);
		brisbane_task_release(dependTaskList[0]);
		asyncTaskMap->erase(default_async);
	} else {
    	brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var), NULL, true);
	}
    brisbane_task_release(task);
#endif
#ifdef _OPENARC_PROFILE_
	tconf->BTaskCnt++;	
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_bind_texref(%s, thread ID = %d) submits a brisbane task2 to the device %d\n", name, threadID, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var));
    }
#endif
	//[DEBUG] Why don't release the task?
    //brisbane_task_release(task);
  } else {
  	threadAsyncMap[threadID] = default_async;
  }

  return HI_success;
}

HI_error_t BrisbaneDriver::HI_create_texobj(void *texObj,  HI_datatype_t type, const void *devPtr, size_t size, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return HI_success;
}

HI_error_t BrisbaneDriver::HI_destroy_texobj(void *texObj,  const void *devPtr, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return HI_success;
}

HI_error_t BrisbaneDriver::HI_memcpy_const(void *hostPtr, std::string constName, HI_MemcpyKind_t kind, size_t count, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return HI_success;
}

HI_error_t BrisbaneDriver::HI_memcpy_const_async(void *hostPtr, std::string constName, HI_MemcpyKind_t kind, size_t count, int async, int num_waits, int *waits, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return HI_success;
}

HI_error_t BrisbaneDriver::HI_present_or_memcpy_const(void *hostPtr, std::string constName, HI_MemcpyKind_t kind, size_t count, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return HI_success;
}

void BrisbaneDriver::HI_set_async(int asyncId, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_set_async(%d)\n", asyncId);
    }    
#endif
	if( unifiedMemSupported == 0 ) {
		//We need explicit synchronization here since HI_synchronize() does not
		//explicitly synchronize if unified memory is not used.
  		HostConf_t * tconf = getHostConf(threadID);
  		threadtaskmapbrisbane_t *asyncTaskMap = threadAsyncTaskMap[threadID];
		int default_async = DEFAULT_QUEUE+tconf->asyncID_offset;
		if( asyncTaskMap->count(default_async) > 0 ) {
#ifdef _OPENARC_PROFILE_
    		if( HI_openarcrt_verbosity > 2 ) {
        		fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_set_async(%d) waits for a brisbane task on async ID = %d\n", asyncId, default_async);
    		}    
#endif
			brisbane_task task = asyncTaskMap->at(default_async);
			brisbane_task_wait(task);
			brisbane_task_release(task);
			asyncTaskMap->erase(default_async);
		}
    }    
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_set_async(%d)\n", asyncId);
    }    
#endif
}

void BrisbaneDriver::HI_set_context(int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_set_context(thread ID = %d)\n", threadID);
    }
#endif
#ifdef PRINT_TODO
  //fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_set_context(thread ID = %d)\n", threadID);
    }
#endif
}

void BrisbaneDriver::HI_wait(int arg, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_wait (arg = %d, thread ID = %d)\n", arg, threadID);
    }
#endif
	int nestingLevel = threadTaskMapNesting[threadID];
	threadtaskmapbrisbane_t *asyncTaskMap = threadAsyncTaskMap[threadID];
	if( nestingLevel != 0 ) {
		fprintf(stderr, "[ERROR in BrisbaneDriver::HI_wait(arg = %d, thread ID = %d)] HI_wait() should not be called inside a Brisbane task region (Brisbane task region nesting level = %d)\n", arg, threadID, nestingLevel);
		exit(1); 
	}
	HostConf_t * tconf = getHostConf(threadID);
	if( asyncTaskMap->count(arg) > 0 ) {
#ifdef _OPENARC_PROFILE_
    	if( HI_openarcrt_verbosity > 2 ) {
        	fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_wait(arg = %d, thread ID = %d) waits for a brisbane task on async ID = %d\n", arg, threadID, arg);
    	}    
#endif
		brisbane_task task = asyncTaskMap->at(arg);
		brisbane_task_wait(task);
		brisbane_task_release(task);
		asyncTaskMap->erase(arg);
		pointerset_t *tmpHostMemSet = threadHostMemFreeMap[threadID];
		if( !(tmpHostMemSet->empty()) ) {
			for(std::set<const void *>::iterator it=tmpHostMemSet->begin(); it!=tmpHostMemSet->end(); ++it) {
				free((void *)*it);
			}
				tmpHostMemSet->clear();
		}
	} else {
		fprintf(stderr, "[ERROR in BrisbaneDriver::HI_wait(arg = %d, thread ID = %d)] HI_wait() there is no Brisbane task to wait for; exit!\n", arg, threadID);
		exit(1); 
	}
	HI_postponed_free(arg, tconf->threadID);
	HI_postponed_tempFree(arg, tconf->acc_device_type_var, tconf->threadID);

#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_wait (arg = %d, thread ID = %d)\n", arg, threadID);
    }
#endif
}

void BrisbaneDriver::HI_wait_ifpresent(int arg, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_wait_ifpresent (arg = %d, thread ID = %d)\n", arg, threadID);
    }
#endif
	int nestingLevel = threadTaskMapNesting[threadID];
	threadtaskmapbrisbane_t *asyncTaskMap = threadAsyncTaskMap[threadID];
	if( nestingLevel != 0 ) {
		fprintf(stderr, "[ERROR in BrisbaneDriver::HI_wait_ifpresent(arg = %d, thread ID = %d)] HI_wait_ifpresent() should not be called inside a Brisbane task region (Brisbane task region nesting level = %d)\n", arg, threadID, nestingLevel);
		exit(1); 
	}
	HostConf_t * tconf = getHostConf(threadID);
	if( asyncTaskMap->count(arg) > 0 ) {
#ifdef _OPENARC_PROFILE_
    	if( HI_openarcrt_verbosity > 2 ) {
        	fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_wait_ifpresent(arg = %d, thread ID = %d) waits for a brisbane task on async ID = %d\n", arg, threadID, arg);
    	}    
#endif
		brisbane_task task = asyncTaskMap->at(arg);
		brisbane_task_wait(task);
		brisbane_task_release(task);
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
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_wait_ifpresent (arg = %d, thread ID = %d)\n", arg, threadID);
    }
#endif
}

void BrisbaneDriver::HI_waitS1(int arg, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
}

void BrisbaneDriver::HI_waitS2(int arg, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
}

void BrisbaneDriver::HI_wait_all(int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_wait_all (thread ID = %d)\n", threadID);
    }
#endif
	int nestingLevel = threadTaskMapNesting[threadID];
	threadtaskmapbrisbane_t *asyncTaskMap = threadAsyncTaskMap[threadID];
	if( nestingLevel != 0 ) {
		fprintf(stderr, "[ERROR in BrisbaneDriver::HI_wait_all(thread ID = %d)] HI_wait_all() should not be called inside a Brisbane task region (Brisbane task region nesting level = %d)\n", threadID, nestingLevel);
		exit(1); 
	}
	HostConf_t * tconf = getHostConf(threadID);
	for( std::map<int, brisbane_task>::iterator it=asyncTaskMap->begin(); it!=asyncTaskMap->end(); ++it ) {
		int arg = it->first;
		brisbane_task task = it->second;
		brisbane_task_wait(task);
		brisbane_task_release(task);
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
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_wait_all (thread ID = %d)\n", threadID);
    }
#endif
}

//[DEBUG] Below implementation is inefficient.
void BrisbaneDriver::HI_wait_async(int arg, int async, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_wait_async (arg = %d, async ID = %d, thread ID = %d)\n", arg, async, threadID);
    }
#endif
	int nestingLevel = threadTaskMapNesting[threadID];
	threadtaskmapbrisbane_t *asyncTaskMap = threadAsyncTaskMap[threadID];
	if( nestingLevel != 0 ) {
		fprintf(stderr, "[ERROR in BrisbaneDriver::HI_wait_async(arg = %d, async ID = %d, thread ID = %d)] HI_wait_async() should not be called inside a Brisbane task region (Brisbane task region nesting level = %d)\n", arg, async, threadID, nestingLevel);
		exit(1); 
	}
	HostConf_t * tconf = getHostConf(threadID);
	if( asyncTaskMap->count(arg) > 0 ) {
#ifdef _OPENARC_PROFILE_
    	if( HI_openarcrt_verbosity > 2 ) {
        	fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_wait_async(arg = %d, async ID = %d, thread ID = %d) waits for a brisbane task on async ID = %d\n", arg, async, threadID, arg);
    	}    
#endif
		brisbane_task task = asyncTaskMap->at(arg);
		brisbane_task_wait(task);
		brisbane_task_release(task);
		asyncTaskMap->erase(arg);
		pointerset_t *tmpHostMemSet = threadHostMemFreeMap[threadID];
		if( !(tmpHostMemSet->empty()) ) {
			for(std::set<const void *>::iterator it=tmpHostMemSet->begin(); it!=tmpHostMemSet->end(); ++it) {
				free((void *)*it);
			}
				tmpHostMemSet->clear();
		}
	} else {
		fprintf(stderr, "[ERROR in BrisbaneDriver::HI_wait_async(arg = %d, async ID = %d, thread ID = %d)] HI_wait_async() there is no Brisbane task to wait for; exit!\n", arg, async, threadID);
		exit(1); 
	}
	HI_postponed_free(arg, tconf->threadID);
	HI_postponed_tempFree(arg, tconf->acc_device_type_var, tconf->threadID);

	if( asyncTaskMap->count(async) > 0 ) {
#ifdef _OPENARC_PROFILE_
    	if( HI_openarcrt_verbosity > 2 ) {
        	fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_wait_async(arg = %d, async ID = %d, thread ID = %d) waits for a brisbane task on async ID = %d\n", arg, async, threadID, async);
    	}    
#endif
		brisbane_task task = asyncTaskMap->at(async);
		brisbane_task_wait(task);
		brisbane_task_release(task);
		asyncTaskMap->erase(async);
	}

#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_wait_async (arg = %d, async ID = %d, thread ID = %d)\n", arg, async, threadID);
    }
#endif
}

void BrisbaneDriver::HI_wait_async_ifpresent(int arg, int async, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_wait_async_ifpresent (arg = %d, async ID = %d, thread ID = %d)\n", arg, async, threadID);
    }
#endif
	int nestingLevel = threadTaskMapNesting[threadID];
	threadtaskmapbrisbane_t *asyncTaskMap = threadAsyncTaskMap[threadID];
	if( nestingLevel != 0 ) {
		fprintf(stderr, "[ERROR in BrisbaneDriver::HI_wait_async_ifpresent(arg = %d, async ID = %d, thread ID = %d)] HI_wait_async_ifpresent() should not be called inside a Brisbane task region (Brisbane task region nesting level = %d)\n", arg, async, threadID, nestingLevel);
		exit(1); 
	}
	HostConf_t * tconf = getHostConf(threadID);
	if( asyncTaskMap->count(arg) > 0 ) {
#ifdef _OPENARC_PROFILE_
    	if( HI_openarcrt_verbosity > 2 ) {
        	fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_wait_async_ifpresent(arg = %d, async ID = %d, thread ID = %d) waits for a brisbane task on async ID = %d\n", arg, async, threadID, arg);
    	}    
#endif
		brisbane_task task = asyncTaskMap->at(arg);
		brisbane_task_wait(task);
		brisbane_task_release(task);
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
        	fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_wait_async_ifpresent(arg = %d, async ID = %d, thread ID = %d) waits for a brisbane task on async ID = %d\n", arg, async, threadID, async);
    	}    
#endif
		brisbane_task task = asyncTaskMap->at(async);
		brisbane_task_wait(task);
		brisbane_task_release(task);
		asyncTaskMap->erase(async);
	}

#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_wait_async_ifpresent (arg = %d, async ID = %d, thread ID = %d)\n", arg, async, threadID);
    }
#endif
}

void BrisbaneDriver::HI_wait_all_async(int async, int threadID) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_wait_all_async (async ID = %d, thread ID = %d)\n",async, threadID);
    }
#endif
	int nestingLevel = threadTaskMapNesting[threadID];
	threadtaskmapbrisbane_t *asyncTaskMap = threadAsyncTaskMap[threadID];
	if( nestingLevel != 0 ) {
		fprintf(stderr, "[ERROR in BrisbaneDriver::HI_wait_all_async(async ID = %d, thread ID = %d)] HI_wait_all_async() should not be called inside a Brisbane task region (Brisbane task region nesting level = %d)\n", async, threadID, nestingLevel);
		exit(1); 
	}
	HostConf_t * tconf = getHostConf(threadID);
	for( std::map<int, brisbane_task>::iterator it=asyncTaskMap->begin(); it!=asyncTaskMap->end(); ++it ) {
		int arg = it->first;
		brisbane_task task = it->second;
		brisbane_task_wait(task);
		brisbane_task_release(task);
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
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_wait_all_async (async ID = %d, thread ID = %d)\n",async, threadID);
    }
#endif
}

int BrisbaneDriver::HI_async_test(int asyncId, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return 1;
//[FIXME] Below implementation is fake.
/*
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_async_test (asyncID = %d, thread ID = %d)\n", asyncID, threadID);
    }
#endif
	int nestingLevel = threadTaskMapNesting[threadID];
	threadtaskmapbrisbane_t *asyncTaskMap = threadAsyncTaskMap[threadID];
	if( nestingLevel != 0 ) {
		fprintf(stderr, "[ERROR in BrisbaneDriver::HI_async_test(asyncID = %d, thread ID = %d)] HI_async_test() should not be called inside a Brisbane task region (Brisbane task region nesting level = %d)\n", asyncID, threadID, nestingLevel);
		exit(1); 
	}
	HostConf_t * tconf = getHostConf(threadID);
	if( asyncTaskMap->count(arg) > 0 ) {
		brisbane_task task = asyncTaskMap->at(arg);
		brisbane_task_wait(task);
		brisbane_task_release(task);
		asyncTaskMap->erase(arg);
		pointerset_t *tmpHostMemSet = threadHostMemFreeMap[threadID];
		if( !(tmpHostMemSet->empty()) ) {
			for(std::set<const void *>::iterator it=tmpHostMemSet->begin(); it!=tmpHostMemSet->end(); ++it) {
				free((void *)*it);
			}
				tmpHostMemSet->clear();
		}
	} else {
		fprintf(stderr, "[ERROR in BrisbaneDriver::HI_async_test(asyncID = %d, thread ID = %d)] HI_async_test() there is no Brisbane task to wait for; exit!\n", asyncID, threadID);
		exit(1); 
	}
	HI_postponed_free(arg, tconf->threadID);
	HI_postponed_tempFree(arg, tconf->acc_device_type_var, tconf->threadID);

#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_async_test (asyncID = %d, thread ID = %d)\n", asyncID, threadID);
    }
#endif
  return 1;
*/
}

int BrisbaneDriver::HI_async_test_ifpresent(int asyncId, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return 1;
}

int BrisbaneDriver::HI_async_test_all(int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
  return 1;
}

void BrisbaneDriver::HI_wait_for_events(int async, int num_waits, int* waits, int threadID) {
#ifdef PRINT_TODO
  fprintf(stderr, "[%s:%d][%s] Not Implemented!\n", __FILE__, __LINE__, __func__);
#endif
}

void BrisbaneDriver::HI_enter_subregion(const char *label, int mode, int threadID) {
  	int nestingLevel = threadTaskMapNesting[threadID];
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_enter_subregion (label = %s, mode = %d, nestlevel = %d, thread ID = %d)\n", label, mode, nestingLevel, threadID);
    }
#endif
  	brisbane_task task;
    if( threadTaskMap.count(threadID) > 0 ) {
  	  task = threadTaskMap[threadID];
    }
	if( nestingLevel == 0 ) {
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_enter_subregion (label = %s, mode = %d, nestlevel = %d, thread ID = %d) creates a brisbane task\n", label, mode, nestingLevel, threadID);
    }
#endif
  		brisbane_task_create(&task);
  		threadTaskMap[threadID] = task;
  		threadAsyncMap[threadID] = NO_QUEUE;
	}
	threadTaskMapNesting[threadID] = ++nestingLevel;
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_enter_subregion (label = %s, mode = %d, nestlevel = %d, thread ID = %d)\n", label, mode, nestingLevel, threadID);
    }
#endif
}

void BrisbaneDriver::HI_exit_subregion(const char *label, int mode, int threadID) {
  	int nestingLevel = threadTaskMapNesting[threadID];
#ifdef _OPENARC_PROFILE_
    if( HI_openarcrt_verbosity > 2 ) {
        fprintf(stderr, "[OPENARCRT-INFO]\t\tenter BrisbaneDriver::HI_exit_subregion (label = %s, mode = %d, nestlevel = %d, thread ID = %d)\n", label, mode, nestingLevel, threadID);
    }
#endif
  	int currentAsync = threadAsyncMap[threadID];
  	brisbane_task task;
    bool task_exist = false;
    if( threadTaskMap.count(threadID) > 0 ) {
  	  task = threadTaskMap[threadID];
      task_exist = true;
    }
	threadtaskmapbrisbane_t *asyncTaskMap = threadAsyncTaskMap[threadID];
    pointerset_t *tmpHostMemSet = threadHostMemFreeMap[threadID];
	nestingLevel--;
	if( nestingLevel <= 0 ) {
		if( task_exist ) {
			int memcpy_cmd_option = 0;
#if OPT_MEMCPY_ONLY_POLICY == 2
			int d2hmemcpy_cmds_cnt = 0;
			int h2dmemcpy_cmds_cnt = 0;
			int ncmds = 0;
			brisbane_task_info(task, brisbane_ncmds, &ncmds, NULL);
			int *cmds = (int *)malloc(ncmds*sizeof(int));
			brisbane_task_info(task, brisbane_cmds, cmds, NULL);
			for(int i=0; i<ncmds; i++) {
				int ctype = cmds[i];
				if( ctype == BRISBANE_CMD_D2H ) {
					d2hmemcpy_cmds_cnt += 1;
				} else if( ctype == BRISBANE_CMD_H2D ) {
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
			brisbane_task_info(task, brisbane_ncmds, &ncmds, NULL);
			int *cmds = (int *)malloc(ncmds*sizeof(int));
			brisbane_task_info(task, brisbane_cmds, cmds, NULL);
			for(int i=0; i<ncmds; i++) {
				int ctype = cmds[i];
				if( ctype == BRISBANE_CMD_D2H ) {
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
#if BRISBANE_TASK_SUBMIT_MODE == 0
#ifdef _OPENARC_PROFILE_
    			if( HI_openarcrt_verbosity > 2 ) {
        			fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_exit_subregion (label = %s, mode = %d, nestlevel = %d, thread ID = %d) submits a brisbane task synchronously to the device %d\n", label, mode, nestingLevel, threadID, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option));
    			}
#endif
    			brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option), NULL, true);
#ifdef _OPENARC_PROFILE_
				tconf->BTaskCnt++;	
#endif
    			brisbane_task_release(task);
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
				if(brisbane_task_kernel_cmd_only(task) != BRISBANE_OK) {
					if( asyncTaskMap->count(currentAsync) > 0 ) {
#ifdef _OPENARC_PROFILE_
    					if( HI_openarcrt_verbosity > 2 ) {
        					fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_exit_subregion (label = %s, mode = %d, nestlevel = %d, thread ID = %d) submits a brisbane task synchronously (async ID = %d) with dependency to the device %d\n", label, mode, nestingLevel, threadID, currentAsync, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option));
    					}
#endif
						brisbane_task dependTaskList[1] = { asyncTaskMap->at(currentAsync) };
						brisbane_task_depend(task, 1, dependTaskList); 
    					brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option), NULL, true);
						brisbane_task_release(dependTaskList[0]);
						asyncTaskMap->erase(currentAsync);
					} else {
#ifdef _OPENARC_PROFILE_
    					if( HI_openarcrt_verbosity > 2 ) {
        					fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_exit_subregion (label = %s, mode = %d, nestlevel = %d, thread ID = %d) submits a brisbane task synchronously (async ID = %d) without dependency to the device %d\n", label, mode, nestingLevel, threadID, currentAsync, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option));
    					}
#endif
    					brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option), NULL, true);
					}
#ifdef _OPENARC_PROFILE_
					tconf->BTaskCnt++;	
#endif
    				brisbane_task_release(task);
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
        					fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_exit_subregion (label = %s, mode = %d, nestlevel = %d, thread ID = %d) submits a brisbane task asynchronously (async ID = %d) with dependency to the device %d\n", label, mode, nestingLevel, threadID, currentAsync, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option));
    					}
#endif
						brisbane_task dependTaskList[1] = { asyncTaskMap->at(currentAsync) };
						brisbane_task_depend(task, 1, dependTaskList); 
    					brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option), NULL, false);
						brisbane_task_release(dependTaskList[0]);
						//asyncTaskMap->erase(currentAsync);
					} else {
#ifdef _OPENARC_PROFILE_
    					if( HI_openarcrt_verbosity > 2 ) {
        					fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_exit_subregion (label = %s, mode = %d, nestlevel = %d,thread ID = %d) submits a brisbane task asynchronously (async ID = %d) without dependency to the device %d\n", label, mode, nestingLevel, threadID, currentAsync, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option));
    					}
#endif
    					brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option), NULL, false);
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
        				fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_exit_subregion (label = %s, mode = %d, nestlevel = %d,thread ID = %d) submits a brisbane task asynchronously (async ID = %d) with dependency to the device %d\n", label, mode, nestingLevel, threadID, currentAsync, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option));
    				}
#endif
					brisbane_task dependTaskList[1] = { asyncTaskMap->at(currentAsync) };
					brisbane_task_depend(task, 1, dependTaskList); 
    				brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option), NULL, false);
					brisbane_task_release(dependTaskList[0]);
				} else {
#ifdef _OPENARC_PROFILE_
    				if( HI_openarcrt_verbosity > 2 ) {
        				fprintf(stderr, "[OPENARCRT-INFO]\t\tBrisbaneDriver::HI_exit_subregion (label = %s, mode = %d, nestlevel = %d,thread ID = %d) submits a brisbane task asynchronously (async ID = %d) without dependency to the device %d\n", label, mode, nestingLevel, threadID, currentAsync, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option));
    				}
#endif
    				brisbane_task_submit(task, HI_getBrisbaneDeviceID(tconf->acc_device_type_var,tconf->user_set_device_type_var, tconf->acc_device_num_var, memcpy_cmd_option), NULL, false);
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
        fprintf(stderr, "[OPENARCRT-INFO]\t\texit BrisbaneDriver::HI_exit_subregion (label = %s, mode = %d, nestlevel = %d, thread ID = %d)\n", label, mode, nestingLevel, threadID);
    }
#endif
}
