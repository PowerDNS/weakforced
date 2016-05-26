#include <unistd.h>

enum PerfStat { WorkerThreadWait_0_1=0, WorkerThreadWait_1_10=2, WorkerThreadWait_10_100=3, WorkerThreadWait_100_1000=4, WorkerThreadWait_Slow=5, WorkerThreadRun_0_1=6, WorkerThreadRun_1_10=7, WorkerThreadRun_10_100=8, WorkerThreadRun_100_1000=9, WorkerThreadRun_Slow=10 }; 

void initPerfStats();
void addWTWStat(unsigned int num_ms); // number of milliseconds
void addWTRStat(unsigned int num_ms); // number of milliseconds
void startStatsThread();
