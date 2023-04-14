-------------------------------------------------------------------------------
RELEASE
-------------------------------------------------------------------------------
OpenARC Runtime V0.75 (April 14, 2023)

OpenARC Runtime implements APIs used by the output program translated
by OpenARC.


-------------------------------------------------------------------------------
REQUIREMENTS
-------------------------------------------------------------------------------
* GCC
* NVCC to run on the CUDA target
* GCC or other OpenCL compiler to run on the OpenCL target

 
-------------------------------------------------------------------------------
INSTALLATION
-------------------------------------------------------------------------------
* Build
  - Go to the parent directory, which is a main entry directory of OpenARC.
  - Create a makefile configuration file, called make.header, for the target
platform, by copying and modifying example configurations in the ./makefiles directory.
    - If Makefile in the main entry directory is used, make.header file will be
    automatically created.
  - Go back to the openarcrt directory 
  - Run "batchmake.bash"
    $ ./batchmake.bash
	//which will create target-specific libraries of the OpenARC runtime, depending
	//on the value of OPENARC_ARCH variable in the make.header file. 
	//For example on a CUDA device (OPENARC_ARCH = 0):
	//libopenaccrt_cuda.a -- normal mode OpenARC CUDA library
	//libopenaccrt_cudapf.a -- profile mode OpenARC CUDA library
	//libopenaccrtomp_cuda.a -- normal mode OpenARC CUDA library with OpenMP support
	//libopenaccrtomp_cudapf.a -- profile mode OpenARC CUDA library with OpenMP support
	//
	//It also creates offline kernel compilation tool.
	//binBuilder_cuda -- for CUDA device (OPENARC_ARCH = 0)
	//binBuilder_hip -- for AMD HIP device (OPENARC_ARCH = 5)
	//binBuilder_iris -- for IRIS device (OPENARC_ARCH = 6)
	//binBuilder_brisbane -- for Brisbane device (OPENARC_ARCH = 6) (deprecated)
	//binBuilder_opencl -- for OpenCL device (Otherwise)
	//
	//The binBuilder tool is optional; if not existing, the kernel file
	//(openarc_kernel.cu or openarc_kernel.cl) will be JIT-compiled 
	//at runtime.

-------------------------------------------------------------------------------
FEATURES/UPDATES
-------------------------------------------------------------------------------
* New features
    - Add a new IRIS driver, which replaces the old Brisbane driver. You can download the new IRIS runtime from the GitHub (https://github.com/ornl/iris).

    - Add a new environment variable, `OPENARCRT_USE_BRISBANE` to enable the old, deprecated Brisbane driver (disabled by default).

    - Add a new environment variable, `OPENARCRT_IRIS_POLICY` to control the task scheduling policy when targeting the IRIS device.

	- Add a new environment variable, `OPENARCRT_PREPINHOSTMEM` to control when the OpenARC runtime performs host-memory-pinning operations.

	- Add a new environment variable, OPENARCRT_HIP_PLATFORM to choose a target device type for OpenARC-generated HIP program.

	- Add a new environment variable, OPENARCRT_BRISBANE_POLICY to control the task scheduling policy when targeting the Brisbane device.

    - Add HIP driver to support AMD HIP devices.

    - Add IRIS/Brisbane driver to support IRIS devices.

    - Add pthread support for multithreading.

        - OpenARC runtime supports three execution modes: single-threaded, OpenMP multithreading, and pthread multithreading

        - If compiled default make option ($ make), pthread version will be created.

            To compile single-threaded version, comment out "#define _THREAD_SAFETY" line in the openaccrt.h file and compile ($ make)

        - If compiled with the OMP option ($ make OMP=1), OpenMP version will be created.

* Updates
	- Update Brisbane/IRIS driver to select different optimization levels for IRIS tasks consisting of memory transfer commands only.

	- Update the texture memory APIs in the CUDA driver to use the CUDA texture object API instead of the deprecated texture reference API.

	- Update IRIS/Brisbane driver to support asynchronous IRIS/Brisbane tasks.

    - Added missing OpenACC runtime API implementations (acc_update_device_async() and acc_update_self_async()).

    - Update the OpenARC runtime to better support different OpenACC device types, which are passed either by an acc_device_t variable or by an environment variable, ACC_DEVICE_TYPE. 

	- Update IRIS/Brisbane driver to merge commands belonging to the same OpenACC directive into a single IRIS/Brisbane task.

* Bug fixes and improvements
	- Disabled texture mrmory support in the HIP driver.

	- Update the HIP driver and related makefiles/make.header.HIP file, so that the deprecated hipCtx_t hipContext is used only when `OPENARCRT_HIP_PLATFORM` environment variable is set to `nvcc`.

	- Fixed bugs in the OpenCL backend to correctly handle multiple platforms.

	- Modified opencldriver so that the same kernel binary name (openarc_kernel.aocx) is used when targeting FPGAs, regardless of actual target FPGAs.

-------------------------------------------------------------------------------
LIMITATIONS
-------------------------------------------------------------------------------
* The generated kernel file (openarc_kernel.cu or openarc_kernel.cl) can be
compiled either offline (using the binBuilder tool or the backend compiler) or 
online (JIT-compiled by the output program).  However, if the output program 
is an MPI program running on a shared file system, the kernel should be 
pre-compiled before the program execution to prevent possible premature loading
of the kernel binary being generated by other MPI task.


The OpenARC Team

URL: http://ft.ornl.gov/research/openarc
EMAIL: lees2@ornl.gov
