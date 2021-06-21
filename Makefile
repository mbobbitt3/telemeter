telemeter: t.c
	clang-10 -Wall -Werror -fopenmp -std=c17 -g -O0 -o telemeter t.c -lhugetlbfs -lomp -lcrypto

run: 
	OMP_PLACES=cores OMP_PROC_BIND=close numactl --physcpubind=2,12 ./telemeter_0xff > 0_ff.R
	OMP_PLACES=cores OMP_PROC_BIND=close numactl --physcpubind=2,12 ./telemeter_0x11 > 0_11.R
	OMP_PLACES=cores OMP_PROC_BIND=close numactl --physcpubind=2,12 ./telemeter_0xff > 1_ff.R
	OMP_PLACES=cores OMP_PROC_BIND=close numactl --physcpubind=2,12 ./telemeter_0xff > 2_ff.R
	OMP_PLACES=cores OMP_PROC_BIND=close numactl --physcpubind=2,12 ./telemeter_0x11 > 1_11.R
	OMP_PLACES=cores OMP_PROC_BIND=close numactl --physcpubind=2,12 ./telemeter_0x11 > 2_11.R
	OMP_PLACES=cores OMP_PROC_BIND=close numactl --physcpubind=2,12 ./telemeter_0xff > 3_ff.R
	OMP_PLACES=cores OMP_PROC_BIND=close numactl --physcpubind=2,12 ./telemeter_0x11 > 3_11.R


