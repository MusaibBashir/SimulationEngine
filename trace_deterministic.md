# Simulation trace

**Seed** 1  
**Termination** EntityLimit(5)  
**Model**

```
arrivals ~ Deterministic(5 values, repeating)
  Server [c=1, FIFO, Deterministic(5 values, repeating), next=exit]
  entry: Server
```


| t | event | entity | station | detail | queue | busy |
|---:|---|---:|---|---|---:|---:|
| 0.0000 | Arrival | 1 | Server | enters the system | 0 | 0 |
| 0.0000 | Seize | 1 | Server | server free, service 3.0000 until 3.0000 | 0 | 1 |
| 2.0000 | Arrival | 2 | Server | enters the system | 0 | 1 |
| 2.0000 | Queue | 2 | Server | all 1 busy, queued at position 1 | 1 | 1 |
| 3.0000 | Exit | 1 | Server | exits; total wait 0.0000, time in system 3.0000 | 1 | 0 |
| 3.0000 | Seize | 2 | Server | pulled from queue after waiting 1.0000, service until 5.0000 | 0 | 1 |
| 5.0000 | Exit | 2 | Server | exits; total wait 1.0000, time in system 3.0000 | 0 | 0 |
| 6.0000 | Arrival | 3 | Server | enters the system | 0 | 0 |
| 6.0000 | Seize | 3 | Server | server free, service 4.0000 until 10.0000 | 0 | 1 |
| 7.0000 | Arrival | 4 | Server | enters the system | 0 | 1 |
| 7.0000 | Queue | 4 | Server | all 1 busy, queued at position 1 | 1 | 1 |
| 10.0000 | Exit | 3 | Server | exits; total wait 0.0000, time in system 4.0000 | 1 | 0 |
| 10.0000 | Seize | 4 | Server | pulled from queue after waiting 3.0000, service until 11.0000 | 0 | 1 |
| 10.0000 | Arrival | 5 | Server | enters the system | 0 | 1 |
| 10.0000 | Queue | 5 | Server | all 1 busy, queued at position 1 | 1 | 1 |
| 11.0000 | Exit | 4 | Server | exits; total wait 3.0000, time in system 4.0000 | 1 | 0 |
| 11.0000 | Seize | 5 | Server | pulled from queue after waiting 1.0000, service until 13.0000 | 0 | 1 |
| 13.0000 | Exit | 5 | Server | exits; total wait 1.0000, time in system 3.0000 | 0 | 0 |

*18 events traced.*
