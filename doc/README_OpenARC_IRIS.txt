[IRIS Setup for OpenARC]
- Build IRIS
	$ export iris=[iris-repository-root-directory]
	$ cd $iris
	$ build.sh    #To eanble IRISD features, build with -DENABLE_MPI=ON -DAUTO_PARALLEL=ON -DAUTO_FLUSH=ON

- Set up environment variables 
	//Set up LD_LIBRARY_PATH, LIBRARY_PATH, CPATH, and PYTHONPATH
	export IRIS_INSTALL_ROOT=[iris-install-directory]
    if [ $HNAME = "leconte" ] || [ $HNAME = "radeon" ] || [ $HNAME = "firefly" ] || [ $HNAME = "pcie" ]; then 
        export IRIS_LIBRARY_PATH=$IRIS_INSTALL_ROOT/lib64
    else 
        export IRIS_LIBRARY_PATH=$IRIS_INSTALL_ROOT/lib
    fi   
    if [ -n "$LD_LIBRARY_PATH" ]; then 
        export LD_LIBRARY_PATH=$IRIS_LIBRARY_PATH:.:$LD_LIBRARY_PATH
    else 
        export LD_LIBRARY_PATH=$IRIS_LIBRARY_PATH:.
    fi   
    if [ -n "$LIBRARY_PATH" ]; then 
        export LIBRARY_PATH=$IRIS_LIBRARY_PATH:$LIBRARY_PATH
    else 
        export LIBRARY_PATH=$IRIS_LIBRARY_PATH
    fi   
    if [ -n "$CPATH" ]; then 
        export CPATH=$IRIS_INSTALL_ROOT/include:$CPATH
    else 
        export CPATH=$IRIS_INSTALL_ROOT/include
    fi   
    if [ -n "$PYTHONPATH" ]; then 
        export PYTHONPATH=$IRIS_INSTALL_ROOT/include:$PYTHONPATH
    else 
        export PYTHONPATH=$IRIS_INSTALL_ROOT/include
    fi   

	//[Optional] Define IRIS_SINGLE if using only one device
	export IRIS_SINGLE=1 

	//[Optional] Set IRIS_ARCHS 
	export IRIS_ARCHS=cuda:hip

[OpenARC Setup]
- Set up environment variables
	//Set OpenARC target architecture
    //OPENARC_ARCH =  0 for CUDA (default)
    //				  1 for general OpenCL (e.g., AMD GPUs)
    //				  2 for Xeon Phi with OpenCL
    //				  3 for Intel FPGA with OpenCL
    //				  4 for MCL with OpenCL
    //				  5 for AMD HIP 
    //				  6 for IRIS 
	//Example setup for the IRIS backend
	export OPENARC_ARCH=6

	//Set openarc with the OpenARC repository root directory
	export openarc=[openarc-repository-root-directory]

	//Set OPENARC_INSTALL_ROOT with the OpenARC install directory
	export OPENARC_INSTALL_ROOT=[openarc-install-directory]

	//For NVIDIA GPUs (the CUDA backend and other backends using NVIDIA GPUs), 
	//CUDA version may need to be adjusted to be compatible with PTX ISA version 
	//supported by the CUDA driver.
	//OpenARC uses nvcc to generate PTX, which is compiled using CUDA driver API.
	//If mismatched, CUDA driver may not be able to compile nvcc-generated PTX. 
	//For example, on Equinox, use CUDA/11.8
	export CUDA_PATH=/opt/nvidia/hpc_sdk/Linux_x86_64/24.9/cuda/11.8

	//Set up PATH, LD_LIBRARY_PATH, LIBRARY_PATH, and CPATH
	export PATH=$PATH:$OPENARC_INSTALL_ROOT/bin
    if [ -n "${CUDA_PATH}" ]; then 
        if [ ${OPENARC_ARCH} -eq 0 ]; then 
            export PATH=$CUDA_PATH/bin:$PATH
            if [ -n "$CPATH" ]; then 
                export CPATH=$CUDA_PATH/include:$CPATH
            else 
                export CPATH=$CUDA_PATH/include
            fi   
        elif [ ${OPENARC_ARCH} -eq 1 ] || [ ${OPENARC_ARCH} -eq 6 ]; then 
            export PATH=$CUDA_PATH/bin:$PATH
            if [ -n "$CPATH" ]; then 
                export CPATH=$CUDA_PATH/include:$CPATH
            else 
                export CPATH=$CUDA_PATH/include
            fi   
            if [ -n "$LD_LIBRARY_PATH" ]; then 
                export LD_LIBRARY_PATH=$CUDA_PATH/lib64:$LD_LIBRARY_PATH
            else 
                export LD_LIBRARY_PATH=$CUDA_PATH/lib64
            fi   
            if [ -n "$LIBRARY_PATH" ]; then 
                export LIBRARY_PATH=$CUDA_PATH/lib64:$LIBRARY_PATH
            else 
                export LIBRARY_PATH=$CUDA_PATH/lib64
            fi   
        fi   
    fi   

	//For AMD GPUs (the HIP backend and other backends using AMD GPUs), 
	//set up LD_LIBRARY_PATH, LIBRARY_PATH, and CPATH
	export ROCM_PATH=/opt/rocm
    if [ -n "${ROCM_PATH}" ]; then 
        if [ ${OPENARC_ARCH} -eq 1 ]; then 
            if [ -n "$CPATH" ]; then 
                export CPATH=${ROCM_PATH}/include:$CPATH
            else 
                export CPATH=${ROCM_PATH}/include
            fi   
            if [ -n "$LD_LIBRARY_PATH" ]; then 
                export LD_LIBRARY_PATH=${ROCM_PATH}/lib:$LD_LIBRARY_PATH
            else 
                export LD_LIBRARY_PATH=${ROCM_PATH}/lib
            fi   
            if [ -n "$LIBRARY_PATH" ]; then 
                export LIBRARY_PATH=${ROCM_PATH}/lib:$LIBRARY_PATH
            else 
                export LIBRARY_PATH=${ROCM_PATH}/lib
            fi   
        fi   
    fi   


- Build OpenARC
	$ cd $openarc
	$ make purge
	$ make INSTALL_PREFIX=${OPENARC_INSTALL_ROOT} #without IRISD features
	or
	$ make TARGET_SYSTEM=IRISD INSTALL_PREFIX=${OPENARC_INSTALL_ROOT} #with IRISD features
	//Without INSTALL_PREFIX, make will assume $openarc/install as the install root.

- Environment variables used by the IRIS backend of the OpenARC runtime:
	//Set whether to use DMEM or not 
	export OPENARCRT_IRIS_DMEM=0 #Do not use DMEM (use IRIS MEM)
							   1 #Use DMEM (default)

	//Set IRIS scheduling policy, which will be applied to all OpenARC-generated IRIS tasks
    export OPENARCRT_IRIS_POLICY=iris_roundrobin
    #if none, do not use any IRIS policy and use OpenACC device type and number when choosing a target device (default).
    #if iris_roundrobin, submit IRIS tasks to devices in a round-robin manner
    #if iris_depend, submit IRIS tasks with dependency to the same device
    #if iris_data, submit IRIS tasks to devices in a way to minimize memory transfers between devices
    #if iris_profile, submit IRIS tasks to devices based on the profiled performance
    #if iris_random, submit IRIS tasks to devices randomly
    #if iris_sdq, submit a IRIS task to a device with minimal pending tasks.
    #if iris_ftf, submit a IRIS task to all devices but only the first available device executes the task. Choose this option to enable IRIS's automatic workload partitioning capability.

[OpenARC task benchmarks]
- Available multi-task versions of OpenACC benchmarks
	export benchmark=${openarc}/test/benchmarks/openacc/kernels/axpy_tasks
	export benchmark=${openarc}/test/benchmarks/openacc/kernels/cg_tasks
	export benchmark=${openarc}/test/benchmarks/openacc/kernels/gemm_tasks
	export benchmark=${openarc}/test/benchmarks/openacc/kernels/jacobi3_tasks
	export benchmark=${openarc}/test/benchmarks/openacc/kernels/mandelbrot_tasks
	export benchmark=${openarc}/test/benchmarks/openacc/kernels/spmul_tasks2
	export benchmark=${openarc}/test/benchmarks/openacc/xsbench_tasks
- Build the task benchmark
	$ cd $benchmark
	$ O2GBuild.script [options]
		//Common O2GBuild.script commandline options
		//GRAPH    #generate IRIS task graph, which is required to use IRISD.
		//MERGE    #apply IRIS-task-merge optimization (default)
		//NOMERGE  #do not apply IRIS-task-merge optimization
		//VERIFY=0 #disable result-verification-pass (default)
		//       1 #enable result-verification-pass
	$ make
- Run the task benchmark
	$ cd $benchmark
	$ cd ./bin
	//axpy_tasks example
	$ axpy_ACC #run non-IRISD version
	$ mpirun -np [num-devices] axpy_ACC -d [num-devices] #run IRISD version
	//Common runtime options available to all benchmark programs.
	//-i <iterations>  #Number of iterations (default: 1)
	//-d <devices>     #Number of devices (default: 1)
	//-c <chunkSize>   #Chunk Size per device (if not set, it will be decided by dividing the input dimension size by <devices>)
	//-am <asyncMode>  #Set the asynchronous Mode (default: 2)
	//                 #if 1, a single queue is used (all tasks are serialized)
	//                 #if 2, <devices> number of queues are used (one queue per device)
	//                 #if 3, assign as many queues as total number of independent tasks.
	//-h               #show available runtime options and exit

	//axpys-specific option to set the input data size
	//-m <arraySize>  Number of array elements, _M_ (default: 1024)
	//Example command
	$ axpy_ACC -m 8192000 -d 4

	//gemm-specific options to set the input data size
	//-m <numElements>  #Number of array elements, _M_ (default: 1024)
	//-n <numElements>  #Number of array elements, _N_ (default: 1024)
	//-k <numElements>  #Number of array elements, _K_ (default: 1024)

	//gemm-specific options to set the input data size
	//gemm benchmark interprets <iterations> differently from other benchmarks.
	//-i <iterations>  #Number of iterations (default: 40000)
	//-r <resolution>  #Value of resolution (default: 5000)

	//xsbench-specific options to set the input data size
	//-s <size>        #Size of H-M Benchmark to run (small, large, XL, XXL)
	//-g <gridpoints>  #Number of gridpoints per nuclide
	//-l <lookups>     #Number of Cross-section (XS) lookups
	//Example command
	$ xsbench_ACC -s small -l 1500000000 -i 1 -d 4
	
