# OpenARC README

## RELEASE

OpenARC V0.74 (March 03, 2023)

Open Accelerator Research Compiler (OpenARC) is a framework built on top of 
the Cetus compiler infrastructure (http://cetus.ecn.purdue.edu), which is 
written in Java for C.  
OpenARC provides extensible environment, where various performance 
optimizations, traceability mechanisms, fault tolerance techniques, etc., 
can be built for better debuggability/performance/resilience on the complex 
accelerator computing. 
OpenARC supports the full feature set of OpenACC V1.0 (+ subset of V2.0) and performs 
source-to-source transformations, targeting heterogeneous devices, such as 
NVIDIA GPUs, AMD GPUs, Intel MICs, and Altera FPGAs.
Please refer to the OpenARC website (https://csmd.ornl.gov/project/openarc-open-accelerator-research-compiler) to 
find more details on OpenARC.

## REQUIREMENTS

1. JAVA SE 7 or later
2. GCC 4.2 or later
3. ANTLRv2 
	Default antlr.jar file is included in this distribution (./lib)
	
## INSTALLATION

- Obtain OpenARC distribution

The latest version of OpenARC can be obtained at:
> https://github.com/ORNL/OpenARC


- Build

First, set up the environment variable, `openarc` to the root directory of this OpenARC repository.

The `openarc` environment variable is needed to compile OpenARC runtime and examples unless `OPENARCINCLUDE` and `OPENARCLIB` environment variables are explicitly set to the directories containing OpenARC header files and libraries, respectively. (See outputs from the "make install" command below to explicitly set up `OPENARCINCLUDE` and `OPENARCLIB` environment variables.)

```shell
$ export openarc=$(PWD)
```

If you want to build OpenARC from the scratch, delete all the temporary files used for previous compiliation, if existing. 
(Below command will also delete ${openarc}/make.header file, if existing.)

```shell
$ make purge
```

Second, create a makefile configuration file, called make.header, for the target
platform in the root directory of this repository, by copying and modifying example 
configurations in the "./makefiles" directory. If you want to use any existing configuration
file in "./makefiles" without any modification, you can use `TARGET_SYSTEM` option
when invoking the make command.

```shell
$ make TARGET_SYSTEM=CUDA #to use ./makefiles/make.header.CUDA as it is.  
```

If you invoke make without the `TARGET_SYSTEM` option, it will use make.header file in the repository root if existing.
If make.header does not exist, it will decide which configuration file to use based on the environment variable, 
`OPENARC_ARCH`, as shown below:

```shell
$ make
```

    will choose ./makefiles/make.header.CUDA if OPENARC_ARCH = 0

                ./makefiles/make.header.OPENCL if OPENARC_ARCH = 1 and if host OS is Linux
             
                ./makefiles/make.header.OSX if OPENARC_ARCH = 1 and if host OS is OSX
             
                ./makefiles/make.header.OPENCL if OPENARC_ARCH = 2
             
                ./makefiles/make.header.INTELFPGA if OPENARC_ARCH = 3
             
                ./makefiles/make.header.HIP if OPENARC_ARCH = 5
             
                ./makefiles/make.header.BRISBANE if OPENARC_ARCH = 6 and OPENARCRT_USE_BRISBANE = 1

                ./makefiles/make.header.IRIS if OPENARC_ARCH = 6


If `OPENARC_ARCH` is not defined, it will guess an appropriate target using some heuristics.

To install OpenARC compiler and runtime, use the following command:

```shell
$ make install INSTALL_PREFIX=[Install directory path]
```

If `INSTALL_PREFIX` is not specified, ./install will be chosen as the default installation prefix.

To build the OpenARC compiler only:

```shell
$ make compile
```

To build the OpenARC runtime only:

```shell
$ make runtime
```

If you build OpenARC from the scratch using the existing configuration file in the "./makefiles" directory, typical build steps would be the following:


```shell
$ export openarc=$(PWD) #Run this command if an environment variable, openarc is not properly set to the root directory of this repository.
$ export OPENARC_ARCH=[target-type-number]
$ make purge
$ make 
```

Here are alternative options to build the OpenARC compiler:

    - For Apache Ant users
        
        The provided build.xml defines the build targets for OpenARC. The available
    targets are "compile", "jar", "bin", "clean" and "javadoc". Users need to edit
    the location of the Antlr tool. (build.xml has not yet been updated to build
    OpenARC's LLVM support.)

    - For Linux/Unix command line users
        
        If LLVM support is desired, first build LLVM and jllvm as described in jllvm/README-openarc.  (LLVM support is necessary only for NVL-C and FITL, and NOT included in the public release version.)

        Run the script build.sh (e.g., $ build.sh bin #compile and create a wrapper script)

    - For SDK (Eclipse, Netbeans, etc) users
    
        First, run "make -f configure.mk base", and build the parser with the
        Antlr tool.

        Then, if LLVM support is desired, run "make -f configure.mk llvm" and build LLVM and jllvm as described in jllvm/README-openarc.  (LLVM support is necessary only for NVL-C and FITL, and NOT included in the release version.)

        Then, follow the instructions of each SDK to set up a project.

To find other way to compile the OpenARC runtime, refer to 
readme_openarcrt.txt in ./openarcrt directory.


## ENVIRONMENT SETUP

- Environment variable, `OPENARC_ARCH`, is used to set a target architecture, 
for which OpenARC translates the input OpenACC program. 
(Default target is NVIDIA CUDA if the variable does not exist.)
It is strongly recommended to explicitly set `OPENARC_ARCH` by users to avoid any possible mismatch between the OpenARC compiler and runtime.

        - Set OPENARC_ARCH = 0 for CUDA (default)

                       1 for general OpenCL (e.g., AMD GPUs)

                       2 for Xeon Phi with OpenCL 

                       3 for Intel/Altera FPGA with OpenCL

                       4 for MCL with OpenCL

                       5 for AMD HIP 

                       6 for IRIS/Birsbane 

  - For example in BASH, 

        export OPENARC_ARCH=0

- To port OpenACC to non-CUDA devices, OpenACC environment variables,
`ACC_DEVICE_TYPE`, should be set to the target device type.
  - For example in BASH, if target device is an AMD GPU,

        export ACC_DEVICE_TYPE=RADEON 

  - Check ./openarcrt/openacc.h to find a list of values of ACC_DEVICE_TYPE supported by OpenARC.

- Environment variable, `OPENARC_JITOPTION`, may be optionally used to pass
options to the backend runtime compiler (NVCC compiler options for JIT CUDA 
kernel compilation or clBuildProgram options for JIT OpenCL kernel compilation).
  - For example, if output OpenCL kernel file (openarc_kernel.cl) contains
  header files, path to the header files may need to be specified to the backend
  OpenCL compiler.

        export OPENARC_JITOPTION="-I . -I $openarc/openarcrt"

- Environment variable, `OPENARC_FPGA`, is used to tell the OpenARC compiler the target FPGA type when `OPENARC_ARCH` is set to 3 (Intel FPGA); otherwise, this variable is ignored by the OpenARC compiler.

        - Set OPENARC_FPGA = STRATIX_10 for Intel Stratix 10 FPGA (default)

						STRATIX_5 for Intel Stratix 5 FPGA

						ARRIA_10 for Intel Arria 10 FPGA

- Environment variable, `OPENARCRT_UNIFIEDMEM`, sets whether to use unified
memory if the underlying device supports.

        if 0, unified memory is disabled (default).

        if 1, use unified memory if the device supports it and appropriate APIs are called.

- Environment variable, `OPENARCRT_VERBOSITY`, is used to set the verbosity
level of profiling by the OpenARC runtime.

        if 0, OpenARC runtime profiler prints the summary information only (default).
    
        if 1, OpenARC runtime profiler prints the entry/exit of OpenACC API calls.
    
        if 2, OpenARC runtime profiler prints the entry/exit of OpenACC API + HeteroIR API calls.
    
        if 3, OpenARC runtime profiler prints the entry/exit of OpenACC API + HeteroIR API + underlying driver API calls.

- Environment variable, `OPENARCRT_MAXMEMPOOLSIZE`, is used to set the maximum
size of the device memory victim cache, which keeps freed device memory for fast reuse.

- Environment variable, `OPENARCRT_MEMORYALIGNMENT`, sets whether to use dynamic
memory alignment optimizations when targeting FPGA OpenCL devices.

        if 0, dynamic memory alignment optimization is disabled.

        if 1, use dynamic memory alignment optimization when targeting FPGA OpenCL devices (default).

- Environment variable, `OPENARCRT_IRIS_POLICY`, is used to decide which IRIS policy to use 
when targeting IRIS devices.

        if none, do not use any IRIS policy and use OpenACC device type and number when choosing a target device (default).

        if iris_roundrobin, submit IRIS tasks to devices in a round-robin manner

        if iris_depend, submit IRIS tasks with dependency to the same device

        if iris_data, submit IRIS tasks to devices in a way to minimize memory transfers between devices

        if iris_profile, submit IRIS tasks to devices based on the profiled performance

        if iris_random, submit IRIS tasks to devices randomly

        if iris_any, submit a IRIS task to a device with minimal pending tasks.

        if iris_all, submit a IRIS task to all devices but only the first available device executes the task.
			Choose this option to enable IRIS's automatic workload partitioning capability, which automatically splits an IRIS task to multiple, independent sub-tasks if the original task consists of affine access patterns only.

- Environment variable, `OPENARCRT_USE_BRISBANE`, is used to use the deprecated Brisbane runtime, instead of the 
new IRIS runtime.

        if 0, use the new IRIS runtime (APIs are prefixed with iris_), instead of the old Brisbane runtime (default).

		if 1, use the old Brisbane runtime (APIs are prefixed with brisbane_).

        if 2, use the new IRIS runtime, but with old Brisbane API (prefixed with brisbane_) available in the new IRIS runtime.

- Environment variable, `OPENARCRT_BRISBANE_POLICY`, is used to decide which brisbane policy to use 
when targeting Brisbane devices. This variable is enabled only when `OPENARCRT_USE_BRISBANE` is set to 1.

        if none, do not use any brisbane policy and use OpenACC device type and number when choosing a target device (default).

        if brisbane_roundrobin, submit Brisbane tasks to devices in a round-robin manner

        if brisbane_depend, submit Brisbane tasks with dependency to the same device

        if brisbane_data, submit Brisbane tasks to devices in a way to minimize memory transfers between devices

        if brisbane_profile, submit Brisbane tasks to devices based on the profiled performance

        if brisbane_random, submit Brisbane tasks to devices randomly

        if brisbane_any, submit a Brisbane task to a device with minimal pending tasks.

        if brisbane_all, submit a Brisbane task to all devices but only the first available device executes the task.
			Choose this option to enable Brisbane's automatic workload partitioning capability, which automatically splits an Brisbane task to multiple, independent sub-tasks if the original task consists of affine access patterns only.

- Environment variable, `OPENARCRT_HIP_PLATFORM`, is used to choose a target device type for the OpenARC-generated HIP program.

        if nvcc, target CUDA GPUs for the OpenARC-generated HIP program

        else, target AMD GPUs for the OpenARC-generated HIP program (default)

- Environment variable, `OPENARCRT_PREPINHOSTMEM`, is used to control when the OpenARC runtime performs host-memory-pinning operations, which are necessary for asynchronous memory transfers when targeting CUDA/HIP devices. Pinning the host memory even for synchronous memory transfers may be able to achieve higher bandwidth than unpinned memory (also known as pagable memory) depending on target systems, but too much prepinning can cause slowdown or crash the program. 

        if 0, the host memory involved in asynchrnous memory transfers is pinned only when first accessed by the HI_memcpy_async API (default).

        if 1, the OpenARC runtime always pre-pins the host memory when its corresponding device mmemory is allocated. 

- To run some examples in "test" directory, environment variable, `openarc`,
should be set to the root directory of this OpenARC package (the directory
where this readme file resides).
  
## RUNNING OpenARC

- Users can run OpenARC in the following way:

```shell
$ java -classpath=[user_class_path] openacc.exec.ACC2GPUDriver [options] [C files]
```

- The "user_class_path" should include the class paths of Antlr and Cetus.
"build.sh" and "build.xml" provides a target (bin) that generates a wrapper script
for OpenARC users; if [openarc-path]/bin/openarc exists, the above command can be shortened as following:

```shell
$ [openarc-path]/bin/openarc [options] [C files]
```

- Or, if OpenARC is installed, you can use the wrapper script in the install directory:

```shell
$ [openarc-install-prefix]/bin/openarc [options] [C files] 
```

- Use addIncludePath option to pass paths for non-standard header files:

```shell
$ [openarc-path]/bin/openarc -addIncludePath=[openarc-runtime-path] [C files]
```

- Use either macro option or "#pragma openarc #define" directive to apply macro definitions to OpenACC/OpenARC annotations; see the LIMITATIONS section.

- When compiled by OpenARC, two macro names (`_OPENACC` and `_OPENARC_`) are implicitly defined, which can be used for conditional compilation.

- Available OpenARC commandline options can be found either in [openarc-path]/test/openarcConf.sample or by running the following command:

```shell
$ [openarc-path]/bin/openarc -dump-options
```

- A recommended way to pass commandline options to OpenARC is to use the sample configuration file ([openarc-path]/test/openarcConf.sample)

	- Copy the openarcConf.sample file to your working directory, modify it as necessary. 

	- Run OpenARC using the gpuConfFile option.
    ```shell
    $ [openarc-path]/bin/openarc -gpuConfFile=openarcConf.sample [C files]
    ```


## TESTING

- "./test" directory contains examples showing how to use OpenARC.
For example, to compile and run matmul.c in ./test/examples/openarc/matmul directory:

```shell
$ cd [openarc-path]/test/examples/openarc/matmul
$ O2Gbuild.script //translate OpenACC to output CUDA or OpenCL program.
$ make            //compile the generated output program.
$ cd bin; matmul_ACC //run the output binary.
```
	
The output kernel file (openarc_kernel.cu or openarc_kernel.cl) can be either pre-compiled 
by the built-in binBuilder tool (binBuilder_cuda or binBuilder_opencl) or JIT-compiled at runtime. 

To JIT-compile the kernel file, be sure to delete any old kernel binary (openarc_kernel_*.ptx).


## FEATURES/UPDATES

- New features
	- Add a new command-line option, enableOpenCLArrayFlattening, which enables the OpenCL array flattening transformation.

	- Add a new IRIS driver, which replaces the old Brisbane driver. You can download the new IRIS runtime from the GitHub (https://github.com/ornl/iris).

	- Add a new environment variable, `OPENARCRT_USE_BRISBANE` to enable the old, deprecated Brisbane driver (disabled by default).

	- Add a new environment variable, `OPENARCRT_IRIS_POLICY` to control the task scheduling policy when targeting the IRIS device.

	- Add a new environment variable, `OPENARCRT_PREPINHOSTMEM` to control when the OpenARC runtime performs host-memory-pinning operations.

	- Add a new environment variable, `OPENARCRT_HIP_PLATFORM` to choose a target device type for OpenARC-generated HIP program.

	- Add a new environment variable, `OPENARCRT_BRISBANE_POLICY` to control the task scheduling policy when targeting the Brisbane device.

	- Add new flags: parallelismMappingStrat and fpgaReduction (refer to ./doc/OpenARC_parallelismmapping.txt regarding the parallelismMappingStrat flag)

	- Add a new environment variable, `OPENARCRT_MEMORYALIGNMENT` to control dynamic memory alignment functionality.

	- Add a basic OpenACC-to-IRIS/Brisbane translation pass as an experimental feature.

	- Update OpenARC parser to support OSX-specific macros.

	- Update OpenARC runtime to be thread-safe for general host-side multithreading; for non-OpenMP-based host multithreading, use SetLogicalThreadID commandline option to pass the runtime a logical thread ID expression, which can be used to distinguish each host thread.

	- Add a basic OpenACC-to-MCL translation pass as an experimental feature.

	- Add several FPGA-specific optimizations (e.g., collapse optimization, reduction optimization, and sliding-window optimization): see ./README_FPGA_example.txt file to learn an example configuration procedure to use the OpenACC-to-FPGA translation framework.

	- Add new flags: expand-user-source and inlineFunctionTransformation

	- Add Altera FPGAs as a new target device

	- Add a fake virtual device address space for OpenCL targets, which allows pointer-arithmetics on the virtual device address for both CUDA and OpenCL devices.

- Updates
	- Update acc2gpu translation pass not to enable context-sensitive kernel cloning if targetting OpenMP or OpenACC without generating device-specific output codes.

	- Update OMP4toACCTranslator pass to correctly handle data clauses and asynchronous OpenMP constructs.

	- Update OMP4toACCTranslator pass to handle worksharing constructs called in device functions.

	- Update OpenMP parser to accept `#pragma openarc #define` directive.

	- Update Brisbane/IRIS driver to select different optimization levels for IRIS tasks consisting of memory transfer commands only.

	- Update the texture memory APIs in the CUDA driver and related code-generation passes to use the CUDA texture object API instead of the deprecated texture reference API.

	- Update IRIS/Brisbane driver to support asynchronous IRIS/Brisbane tasks.

	- Added missing OpenACC runtime API implementations (acc_update_device_async() and acc_update_self_async()).

	- Update the OpenARC runtime to better support different OpenACC device types, which are passed either by an acc_device_t variable or by an environment variable, `ACC_DEVICE_TYPE`. 

	- Update HIP driver to use the latest version of HIP.

	- Update IRIS/Brisbane output generation pass and IRIS/Brisbane driver to merge commands belonging to the same OpenACC directive into a single IRIS/Brisbane task.

    - Update CUDA runtime to allow intermixing of both OpenACC and CUDA. (See example in [openarc-path]/test/examples/openarc/matmul_openacc_cuda)

- Bug fixes and improvements
	- Fixed bugs in device function handling passes.

	- Fixed bugs in the vector-to-worker clause tranformation pass.

	- Fixed bugs in the SymbolTools.getSymbolOf() API to better handle the cases for dereferencing binary expressions such as `*(p+i)`.

	- Fixed bugs in the OMP4toACCTranlator pass regarding handling OpenMP depend clauses.

	- Fixed bugs in the OpenCL address space qualifier modification pass.

	- Fix bugs in the OpenACC-to-CUDA translator in handling wait clauses.

	- Fixed bugs in the acc2gpu.java to correctly handle fixed size integer type in OpenCL kernels.

	- Fixed bugs in the array-caching-on-register optimization when handling gang-only outermost loops.

	- Update the compiler passes not to use deprecated `new <WrapperType>(<primitiveType>)` APIs.

	- Update the OpenARC runtime bugs in handling device memory victim cache.

	- Update the OpenARC C parser to support additional types in C99/C11 and CUDA/OpenCL.

	- Fixed bugs in the gang-private variable transformation and worksharing-loop transformation.

	- Fixed bugs in the OpenCL backend to correctly handle multiple platforms.

	- Fixed bugs in privatization and reduction transformation passes.

	- Fixes various bugs related to multi-threading and synchronizations.

	- OpenACC update directives allow subarrays with non-zero start index, which 
offers partial-array transfers between the host and device.

	- Fix bugs in setting an OpenCL driver for MICs.

- Updates in flags


## CONTENTS

- This OpenARC release has the following contents.

    - README.md     - This file
    
    - README_FPGA_example.txt     - Example configuration setup procedure to use OpenACC-to-FPGA framework
    
    - Makefile               - Makefile to build both OpenARC compiler and runtime
    
    - configure.mk           - Configuration file used by Makefile
    
    - makefiles              - Target-specific makefile configuration examples used by Makefile
    
    - make.template          - Configuration file used to compile OpenARC-generated output program.
    
    - build.sh               - Command line build script to compile OpenARC compiler
    
    - build.xml              - Build configuration for Apache Ant to compile OpenARC compiler
    
    - lib                    - Archived classes (jar)
    
    - batchCleanup.bash      - Global cleanup script
    
    - src                    - OpenARC source code
    
    - doc                    - OpenARC documents
    
    - openarcrt              - OpenARC runtime (HeteroIR) source code
    
    - llvm                   - LLVM sources
    
    - jllvm                  - Java bindings for LLVM
    
    - test                   - Examples showing how to use OpenARC


## REFERENCE

To cite OpenARC, please use the following papers:

    Seyong Lee and Jeffrey S. Vetter, OpenARC: Open Accelerator Research Compiler for Directive-Based, Efficient Heterogeneous Computing, HPDC: International ACM Symposium on High-Performance Parallel and Distributed Computing, Short Paper, June 2014

    Seyong Lee and Jeffrey S. Vetter, OpenARC: Extensible OpenACC Compiler Framework for Directive-Based Accelerator Programming Study, WACCPD: Workshop on Accelerator Programming Using Directives in Conjunction with SC'14, November 2014

Bibtex files for the above papers and other OpenARC-related research can be found
in the OpenARC website (http://ft.ornl.gov/research/openarc).


## LIMITATIONS

- The underlying C parser in the current implementation supports C99 
features only partially. If parsing errors occur for the valid input 
C program, the program may contain unsupported C99 features.
    - One of C99 features not fully supported in the current implementation 
	is mixed declaration and code; to fix this, put all variable declaration 
	statements in a function before all code sections in the function body.
        - Example: change "int a; a = 1; int b;" to "int a; int b; a = 1;".

- The current OpenARC implementation follows the memory layout requirement of OpenACC V1.0, which enforces that data in OpenACC data clauses should be contiguous 
in the memory. This means that double/triple pointers (e.g., float \*\*p) are not
allowed. One way to allocate 2D array in a contiguous way is the following:

		float (*a)[N] = (float (*)[N])malloc(sizeof(float) * M * N);
			
		//Where N should be compile-time constant.
		
		...
		
		a[i][j] = c; //a can be accessed as if 2D array.

- C preprocessor in the current implementation does not expand macros 
in pragma annotations. To enable this, either 1) use the OpenARC commandline option, macro (e.g., -macro=SIZE=1024) or 2) use "#pragma openarc #define macro value" 
directive in the input source code.

- The current implementation does not support a Struct member in OpenACC data clauses; one way to avoid this problem is to manually decompose struct data.

    - Example: change "struct var { int *data1; int *data2;} aa;" to "int *aa_data1; int *aa_data2;"
    
- The current implementation allows a subarray (partial array) if its start index is 0; subarrays with non-zero start index are allowed only in an OpenACC update directive.

- The current implementation does not support Variable Length Arrays (VLAs).
Even if a host variable is a pointer, it will be changed to VLA if it is used as a private variable in a kernel. To prevent this, the number of elements of data pointed by the pointer should be changed to compile-time constant. 

	- Example: in the following example, N should be compile-time constant.

            #pragma acc parallel loop private(z[0:N]) ...

- Current implementation ignores vector clause. (e.g., for CUDA target, 
gang clause is used to set the number of thread blocks, and worker clause is 
used to specify the number of  threads in a thread block.) If a compute region 
has only gang clauses without any worker clause (ignoring vector clauses), 
the compiler may automatically add worker clause where appropriate.

- OpenACC standard assumes that there is an implicit barrier at the end of
each compute region if the async clause is not present, but the OpenARC runtime
may skip the implicit barrier if safe. To disable this optimization, which may be
necessary for correct timing of the compute region execution time, set the
forceSyncKernelCall commandline option to 1.

- Current implementation allows data regions to have compute regions 
interprocedurally, but a compute region can have a function call only if 
the called function does not contain any OpenACC loop directive.  

- Current implementation mistakenly moves any struct, union, or enum
definition from a function parameter list to the enclosing scope instead
of treating it as belonging to the function's scope.  Such definitions are
often considered bad practice anyway and normally produce warnings from gcc
or clang.

- Current implementation fails to enter into its symbol table any struct or
union that is referenced (e.g., "struct S \*s;") but never explicitly
declared (e.g., "struct S;") or defined (e.g., "struct S { int i; };").
Such usage is permitted in C and can occur when a struct or union is
intended to remain opaque within a translation unit.  So far, this bug does
not appear to impact OpenARC's user-visible behavior, but it may impact code
extending OpenARC.  A workaround is to forward-declare such a struct or
union before referencing it.

- In current implementation, C parser recognizes all directives as standalone 
annotations, which will be incorrect if they are non-standalone types. This error is corrected by the following OpenACC/OpenMP parser, which re-attaches non-standalone 
directives to structured blocks. The two-step parsing may not work if either for-loop or if-statement has only one child statement without using bracket. Therefore, the following examples will be parsed incorrectly:

	//Example1

	//C parser will incorrectly parse the i-loop having the OpenACC directive as its body, since the C parser recognizes all directives as standalone types.

		for(i=0; i<N; i++)
	
		#pragma acc parallel loop
	
		for(j=0; j<M; j++) { ...//kernel loop body }

	//C parser will parse the i-loop correctly.

		for(i=0; i<N; i++) {
	
		#pragma acc parallel loop
	
		for(j=0; j<M; j++) { ...//kernel loop body }
	
		}

	//Example2

	//C parser will incorrectly parse the if-statement.

		if(cond) 

			#pragma acc parallel loop

			for(j=0; j<M; j++) { ...//kernel loop body }

	//C parser will correctly parse the if-statement.

		if(cond) {

			#pragma acc parallel loop

			for(j=0; j<M; j++) { ...//kernel loop body }

		}

The OpenARC Team

URL: http://ft.ornl.gov/research/openarc

EMAIL: lees2@ornl.gov
