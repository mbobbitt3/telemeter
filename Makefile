telemeter: t.c
	clang-10 -Wall -Werror -fopenmp -std=c17 -g -O0 -o telemeter t.c -lhugetlbfs -lomp -lcrypto

telmod: tel_mod.c
	clang-10 -Wall -fopenmp -std=c17 -g -O0 -o tel_mod tel_mod.c -lomp -lhugetlbfs -lcrypto

telecb: tel_ecb.c
	clang-10 -Wall -Werror -fopenmp -std=c17 -g -O0 -o tel_ecb tel_ecb.c -lhugetlbfs -lomp -lcrypto
	
telpg: tel_pg_threading.c
	clang-10 -Wall -Werror -fopenmp -std=c17 -g -O0 -o tel_pg tel_pg_threading.c -lhugetlbfs -lomp -lcrypto	
run: 
	OMP_PLACES=cores OMP_PROC_BIND=close numactl --physcpubind=2,12 ./telemeter > 0_ff.R
	OMP_PLACES=cores OMP_PROC_BIND=close numactl --physcpubind=2,12 ./telemeter > 0_11.R
	OMP_PLACES=cores OMP_PROC_BIND=close numactl --physcpubind=2,12 ./telemeter > 1_ff.R
	OMP_PLACES=cores OMP_PROC_BIND=close numactl --physcpubind=2,12 ./telemeter > 2_ff.R
	OMP_PLACES=cores OMP_PROC_BIND=close numactl --physcpubind=2,12 ./telemeter > 1_11.R
	OMP_PLACES=cores OMP_PROC_BIND=close numactl --physcpubind=2,12 ./telemeter > 2_11.R
	OMP_PLACES=cores OMP_PROC_BIND=close numactl --physcpubind=2,12 ./telemeter > 3_ff.R
	OMP_PLACES=cores OMP_PROC_BIND=close numactl --physcpubind=2,12 ./telemeter > 3_11.R


run21: 
	OMP_NUM_THREADS=2 OMP_PLACES=cores OMP_PROC_BIND=close numactl --physcpubind=13,14 ./tel_mod > 0_ff.R
#	OMP_PLACES=cores OMP_PROC_BIND=close taskset --cpu-list 2 ./tel_mod > 0_ff.R

run31: 
	OMP_PLACES=cores OMP_PROC_BIND=close numactl --physcpubind=2,12 ./tel_ecb > 0_ff.R
	OMP_PLACES=cores OMP_PROC_BIND=close numactl --physcpubind=2,12 ./tel_ecb > 0_11.R
	OMP_PLACES=cores OMP_PROC_BIND=close numactl --physcpubind=2,12 ./tel_ecb > 1_ff.R
	OMP_PLACES=cores OMP_PROC_BIND=close numactl --physcpubind=2,12 ./tel_ecb > 2_ff.R
	OMP_PLACES=cores OMP_PROC_BIND=close numactl --physcpubind=2,12 ./tel_ecb > 3_ff.R

run41: 
	OMP_PLACES=cores OMP_PROC_BIND=close numactl --physcpubind=13,14,15 ./tel_pg > 0_ff.R
clean:
	rm *.R telemeter tel_mod tel_ecb tel_pg
