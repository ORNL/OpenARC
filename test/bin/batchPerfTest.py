#!/usr/bin/python
import os
import subprocess
import shlex

#Set the number of executions for each benchmark.
NUM_REPEATS=1

OPENARC=os.getenv('openarc')
#print("openarc env = %s" % (OPENARC))
if OPENARC == None :
    print("[ERROR] Cannot find the definition of an environment variable, openarc; exit");
    quit()

SKIP_RODINIA=0
RODINIA=os.getenv('rodinia')
#print("rodinia env = %s" % (RODINIA))
if RODINIA == None :
    print("[ERROR] Cannot find the definition of an environment variable, rodinia; skip related tests");
    SKIP_RODINIA=1

WORKDIR=os.path.dirname(os.path.abspath(__file__))
#print("[INFO] Current working directory: %s" % (WORKDIR))

TEST_TARGETS=[["jacobi","/test/benchmarks/openacc/kernels/jacobi","O2GBuild.script 16384","make","jacobi_ACC"," "],
        ["laplace2d","/test/benchmarks/openacc/kernels/laplace2d","O2GBuild.script","make","laplace2d_ACC", " "],
        ["mandelbrot","/test/benchmarks/openacc/kernels/mandelbrot","O2GBuild.script 16384","make","mandelbrot_ACC", " "],
        ["matmul","/test/benchmarks/openacc/kernels/matmul","O2GBuild.script 16384","make","matmul_ACC", " "],
        ["matmul_openacce","/test/benchmarks/openacc/kernels/matmul_openacce","O2GBuild.script 16384","make","matmul_ACC", " "],
        ["spmul","/test/benchmarks/openacc/kernels/spmul","O2GBuild.script","make","spmul_ACC", " "],
        ["npb-cg","/test/benchmarks/openacc/npb/cg","O2GBuild.script C","make","cg_ACC", " "],
        ["npb-ep","/test/benchmarks/openacc/npb/ep","O2GBuild.script C","make","ep_ACC", " "],
        ["npb-ft","/test/benchmarks/openacc/npb/ft","O2GBuild.script B","make","ft_ACC", " "],
        ["npb-cg_sp","/test/benchmarks/openacc/npb_singleprec/cg","O2GBuild.script C","make","cg_ACC", " "],
        ["npb-ep_sp","/test/benchmarks/openacc/npb_singleprec/ep","O2GBuild.script C","make","ep_ACC", " "],
        ["npb-ft_sp","/test/benchmarks/openacc/npb_singleprec/ft","O2GBuild.script B","make","ft_ACC", " "],
        ["randles","/test/benchmarks/openacc/randles","O2GBuild.script","make","randlesminibench_ACC", " "],
        ["backprop","/test/benchmarks/openacc/rodinia/backprop","O2GBuild.script 6553600","make I_SIZE=6553601","backprop_ACC", "6553600"],
        ["bfs","/test/benchmarks/openacc/rodinia/bfs","O2GBuild.script 16777216 100671006","make NUM_OF_NODES=16777216 EDGELIST_SIZE=100671006","bfs_ACC", "1 " + RODINIA + "/data/bfs/graph16M.txt"],
        ["cfd","/test/benchmarks/openacc/rodinia/cfd","O2GBuild.script 232536 128 232576","make NEL=232536 BLOCK_LENGTH=128 NELR=232576","cfd_ACC", RODINIA + "/data/cfd/missile.domn.0.2M"],
        ["hotspot","/test/benchmarks/openacc/rodinia/hotspot","O2GBuild.script 1024","make ROW_SIZE=1024 COL_SIZE=1024","hotspot_ACC", "1024 1024 100 1 " + RODINIA + "/data/hotspot/temp_1024 " + RODINIA + "/data/hotspot/power_1024 hpoutput_1024.txt"],
        ["kmeans","/test/benchmarks/openacc/rodinia/kmeans","O2GBuild.script 5 34 204800 1","make _NCLUSTERS=5 _NATTRIBUTES=34 _NPOINTS=204800 _UNROLLFAC=1","kmeans_ACC", "-i " + RODINIA + "/data/kmeans/204800.txt"],
        ["lud","/test/benchmarks/openacc/rodinia/lud","O2GBuild.script 4194304","make","lud_ACC", "-v -i " + RODINIA + "/data/lud/2048.dat"],
        ["nw","/test/benchmarks/openacc/rodinia/nw","O2GBuild.script 16385","make","nw_ACC", "16384 10 1"],
        ["srad","/test/benchmarks/openacc/rodinia/srad","O2GBuild.script 8192 0","make","srad_ACC", "8192 8192 0 127 0 127 1 0.5 100"],
        ["epcc","/test/benchmarks/openacc/epcc","O2GBuild.script 67108864","make","epcc_ACC", "--reps 3 --datasize 67108864"],
        ["lulesh","/test/benchmarks/openacc/lulesh","O2GBuild.script","make","lulesh_ACC", "-s 100 -i 1000"],
        ["spec-xsbench","/test/benchmarks/openacc/xsbench","O2GBuild.script","make","xsbench_ACC", "-s small -l 100000000"],
        ["spec-ostencil","/test/benchmarks/openacc/specaccel1.1/303.ostencil/src","O2GBuild.script","make","ostencil_ACC", "512 512 98 20000"],
        ["spec-olbm","/test/benchmarks/openacc/specaccel1.1/304.olbm/src","O2GBuild.script ref","make","lbm_ACC", "5000 reference.dat 0 0 100_100_130_ldc.of"],
        ["spec-omriq","/test/benchmarks/openacc/specaccel1.1/314.omriq/src","O2GBuild.script ref","make","mriq_ACC", "-i ../../data/ref/128x128x128.bin -o 128x128x128.out 200000"],
        ["spec-ep","/test/benchmarks/openacc/specaccel1.1/352.ep/src","O2GBuild.script ref","make","ep_ACC", " "],
        ["spec-cg","/test/benchmarks/openacc/specaccel1.1/354.cg/src","O2GBuild.script ref","make","cg_ACC", " "],
        ["spec-csp","/test/benchmarks/openacc/specaccel1.1/357.csp/src","O2GBuild.script test","make","sp_ACC", " "],
        ["spec-bt","/test/benchmarks/openacc/specaccel1.1/370.bt/src","O2GBuild.script train","make","bt_ACC", " "]]

SKIP_TESTS=["None"]

oFILE = open(WORKDIR+"/PerfTestResults.txt", "w")
eFILE1 = open(WORKDIR+"/PerfTestErrors.txt", "w")
eFILE2 = open(WORKDIR+"/PerfTestErrorOutputs.txt", "w")

dPROCESS=subprocess.Popen("date", stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
stdout, stderr = dPROCESS.communicate()
tDATE = "==> Test starts: " + stdout
eFILE1.write(tDATE)

PROG_NAMES=[]
PROG_TIMES=[]
ePROG_NAMES=[]
ePROG_PATHS=[]
eOUTPUTS=[]
testSize=len(TEST_TARGETS)
for TEST in TEST_TARGETS:
	PROGNAME=TEST[0]
	if PROGNAME in SKIP_TESTS:
		continue
	PROGPATH=OPENARC+TEST[1]
	BUILDCMD=TEST[2]
	MAKECMD=TEST[3]
	RUNCMD=TEST[4]
	RUNCMDOPT=TEST[5]
	tARGS=shlex.split(RUNCMDOPT)
	tCMDARRAY=[RUNCMD] + tARGS
	#print("PROGNAME = %s" % (PROGNAME))
	#print("PROGPATH = %s" % (PROGPATH))
	#print("BUILDCMD = %s" % (BUILDCMD))
	#print("MAKECMD = %s" % (MAKECMD))
	#print("RUNCMD = %s" % (RUNCMD))
	os.chdir(PROGPATH)
	tPATH=os.getcwd()
	if SKIP_RODINIA == 1:
		if PROGNAME == "bfs" or PROGNAME == "hotspot" or PROGNAME == "cfd" or PROGNAME == "kmeans" or PROGNAME == "lud":
			ePROG_NAMES.append(PROGNAME)
			ePROG_PATHS.append(PROGPATH)
			tSTRING = PROGNAME+"\t"+PROGPATH+"\n"
			eFILE1.write(tSTRING)
			eFILE1.flush()
			continue
	subprocess.call("make purge", shell=True)
	subprocess.call(BUILDCMD, shell=True)
	subprocess.call(MAKECMD, shell=True)
	os.chdir("./bin")
	if os.path.isfile(RUNCMD):
		tPROGFOUND = 0
		tREPEATS = range(NUM_REPEATS)
		for cnt in tREPEATS:
			tPROCESS=subprocess.Popen(tCMDARRAY, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
			stdout, stderr = tPROCESS.communicate()
			tOUTPUTARRAY = stdout.split("\n")
			for tELEMENT in tOUTPUTARRAY:
				if (tELEMENT.find("[OPENARC-PROFILE]") != -1):
					if PROGNAME == "epcc":
						print(tELEMENT)
						tWORDS=tELEMENT.split(" ")
						tPROGNAME=tWORDS[5]
						tEXETIME=tWORDS[len(tWORDS)-2]
						if cnt == 0:
							#tSTRING = PROGNAME+"-"+tPROGNAME+"\t"+tEXETIME
							PROG_NAMES.append(PROGNAME+"-"+tPROGNAME)
							PROG_TIMES.append(tEXETIME)
						else:
							#tSTRING = "\t"+tEXETIME
							PROG_TIMES.append(tEXETIME)
						#oFILE.write(tSTRING)
						#oFILE.flush()
						tPROGFOUND = 1
					else:
						print(tELEMENT)
						tWORDS=tELEMENT.split(" ")
						tEXETIME=tWORDS[len(tWORDS)-2]
						if cnt == 0:
							tSTRING = PROGNAME+"\t"+tEXETIME
							#PROG_NAMES.append(PROGNAME)
							#PROG_TIMES.append(tEXETIME)
						else:
							tSTRING = "\t"+tEXETIME
							#PROG_TIMES.append(tEXETIME)
						oFILE.write(tSTRING)
						oFILE.flush()
						tPROGFOUND = 1
		if tPROGFOUND == 1:
			if PROGNAME == "epcc":
				progSize = len(PROG_NAMES)
				timeSize = len(PROG_TIMES)
				nCNT = 0;
				for tNAME in PROG_NAMES:
					tSTRING = tNAME;
					tCNT = nCNT;
					while tCNT < timeSize:
						tSTRING = tSTRING+"\t"+PROG_TIMES[tCNT]
						tCNT += progSize
					oFILE.write(tSTRING+"\n")
					oFILE.flush()
					nCNT += 1
			else:
				oFILE.write("\n")
				oFILE.flush()
		else:
			for cnt in tREPEATS:
				if cnt == 0:
					tSTRING = PROGNAME+"\t999998"
				else:
					tSTRING = "\t999998"
				oFILE.write(tSTRING)
				oFILE.flush()
			oFILE.write("\n")
			ePROG_NAMES.append(PROGNAME)
			ePROG_PATHS.append(PROGPATH)
			tSTRING = PROGNAME+"\t"+PROGPATH+"\n"
			eFILE1.write(tSTRING)
			eFILE1.flush()
			eOUTPUTS.append("Benchmark: "+PROGNAME)
			eFILE2.write("Benchmark: "+PROGNAME+"\n")
			for tCMD in tCMDARRAY:
				eFILE2.write(tCMD+"\n")
			for tELEMENT in tOUTPUTARRAY:
				eOUTPUTS.append(tELEMENT)
				tSTRING = tELEMENT+"\n"
				eFILE2.write(tSTRING)
	else:
		for cnt in tREPEATS:
			if cnt == 0:
				tSTRING = PROGNAME+"\t999999"
			else:
				tSTRING = "\t999999"
			oFILE.write(tSTRING)
			oFILE.flush()
		oFILE.write("\n")
		ePROG_NAMES.append(PROGNAME)
		ePROG_PATHS.append(PROGPATH)
		tSTRING = PROGNAME+"\t"+PROGPATH+"\n"
		eFILE1.write(tSTRING)
		eFILE1.flush()
		eOUTPUTS.append("Benchmark: "+PROGNAME)
		eFILE2.write("Benchmark: "+PROGNAME+"\n")
		eFILE2.write("No binary found: " + RUNCMD + "\n")

os.chdir(WORKDIR)
dPROCESS=subprocess.Popen("date", stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
stdout, stderr = dPROCESS.communicate()
tDATE = "==> Test ends: " + stdout
eFILE1.write(tDATE)
oFILE.close()
eFILE1.close()
eFILE2.close()

