# Simulation trace

**Seed** 7  
**Termination** AnyOf[TimeLimit(6000) | EntityLimit(100000)]  
**Model**

```
arrivals ~ Exponential(mean=15)
  Host [c=1, FIFO, service Exponential(mean=2), next=Waiters]
  Waiters [c=3, FIFO, service Triangular(20, 35, 60), next=Cashier]
  Cashier [c=1, Highest(priority), service Uniform(1, 4), next=exit]
  entry: Host
```


| t | event | entity | station | detail | queue | busy |
|---:|---|---:|---|---|---:|---:|
| 0.0000 | Arrival | 1 | Host | enters the system | 0 | 0 |
| 0.0000 | Seize | 1 | Host | server free, service 0.7683 until 0.7683 | 0 | 1 |
| 0.7683 | Move | 1 | Host | service done, routing to Waiters | 0 | 0 |
| 0.7683 | Seize | 1 | Waiters | server free, service 55.3334 until 56.1017 | 0 | 1 |
| 3.8687 | Arrival | 2 | Host | enters the system | 0 | 0 |
| 3.8687 | Seize | 2 | Host | server free, service 0.7364 until 4.6051 | 0 | 1 |
| 4.6051 | Move | 2 | Host | service done, routing to Waiters | 0 | 0 |
| 4.6051 | Seize | 2 | Waiters | server free, service 32.5826 until 37.1877 | 0 | 2 |
| 12.9894 | Arrival | 3 | Host | enters the system | 0 | 0 |
| 12.9894 | Seize | 3 | Host | server free, service 1.0873 until 14.0767 | 0 | 1 |
| 14.0767 | Move | 3 | Host | service done, routing to Waiters | 0 | 0 |
| 14.0767 | Seize | 3 | Waiters | server free, service 23.0897 until 37.1664 | 0 | 3 |
| 14.3505 | Arrival | 4 | Host | enters the system | 0 | 0 |
| 14.3505 | Seize | 4 | Host | server free, service 4.0621 until 18.4125 | 0 | 1 |
| 18.4125 | Move | 4 | Host | service done, routing to Waiters | 0 | 0 |
| 18.4125 | Queue | 4 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 25.6046 | Arrival | 5 | Host | enters the system | 0 | 0 |
| 25.6046 | Seize | 5 | Host | server free, service 0.9983 until 26.6029 | 0 | 1 |
| 26.6029 | Move | 5 | Host | service done, routing to Waiters | 0 | 0 |
| 26.6029 | Queue | 5 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 31.6306 | Arrival | 6 | Host | enters the system | 0 | 0 |
| 31.6306 | Seize | 6 | Host | server free, service 2.2314 until 33.8620 | 0 | 1 |
| 33.8620 | Move | 6 | Host | service done, routing to Waiters | 0 | 0 |
| 33.8620 | Queue | 6 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 37.1664 | Move | 3 | Waiters | service done, routing to Cashier | 3 | 2 |
| 37.1664 | Seize | 4 | Waiters | pulled from queue after waiting 18.7538, service until 79.6744 | 2 | 3 |
| 37.1664 | Seize | 3 | Cashier | server free, service 2.0379 until 39.2043 | 0 | 1 |
| 37.1877 | Move | 2 | Waiters | service done, routing to Cashier | 2 | 2 |
| 37.1877 | Seize | 5 | Waiters | pulled from queue after waiting 10.5848, service until 88.7930 | 1 | 3 |
| 37.1877 | Queue | 2 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 39.2043 | Exit | 3 | Cashier | exits; total wait 0.0000, time in system 26.2149 | 1 | 0 |
| 39.2043 | Seize | 2 | Cashier | pulled from queue after waiting 2.0166, service until 40.9920 | 0 | 1 |
| 40.9920 | Exit | 2 | Cashier | exits; total wait 2.0166, time in system 37.1233 | 0 | 0 |
| 48.4587 | Arrival | 7 | Host | enters the system | 0 | 0 |
| 48.4587 | Seize | 7 | Host | server free, service 0.5885 until 49.0471 | 0 | 1 |
| 49.0471 | Move | 7 | Host | service done, routing to Waiters | 0 | 0 |
| 49.0471 | Queue | 7 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 56.1017 | Move | 1 | Waiters | service done, routing to Cashier | 2 | 2 |
| 56.1017 | Seize | 6 | Waiters | pulled from queue after waiting 22.2397, service until 103.9072 | 1 | 3 |
| 56.1017 | Seize | 1 | Cashier | server free, service 1.5222 until 57.6239 | 0 | 1 |
| 57.6239 | Exit | 1 | Cashier | exits; total wait 0.0000, time in system 57.6239 | 0 | 0 |
| 69.2989 | Arrival | 8 | Host | enters the system | 0 | 0 |
| 69.2989 | Seize | 8 | Host | server free, service 5.5493 until 74.8482 | 0 | 1 |
| 74.8482 | Move | 8 | Host | service done, routing to Waiters | 0 | 0 |
| 74.8482 | Queue | 8 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 79.6744 | Move | 4 | Waiters | service done, routing to Cashier | 2 | 2 |
| 79.6744 | Seize | 7 | Waiters | pulled from queue after waiting 30.6273, service until 116.1973 | 1 | 3 |
| 79.6744 | Seize | 4 | Cashier | server free, service 2.1539 until 81.8284 | 0 | 1 |
| 81.8284 | Exit | 4 | Cashier | exits; total wait 18.7538, time in system 67.4779 | 0 | 0 |
| 88.7930 | Move | 5 | Waiters | service done, routing to Cashier | 1 | 2 |
| 88.7930 | Seize | 8 | Waiters | pulled from queue after waiting 13.9447, service until 123.4383 | 0 | 3 |
| 88.7930 | Seize | 5 | Cashier | server free, service 1.5801 until 90.3730 | 0 | 1 |
| 90.3730 | Exit | 5 | Cashier | exits; total wait 10.5848, time in system 64.7684 | 0 | 0 |
| 92.7633 | Arrival | 9 | Host | enters the system | 0 | 0 |
| 92.7633 | Seize | 9 | Host | server free, service 1.3180 until 94.0813 | 0 | 1 |
| 94.0813 | Move | 9 | Host | service done, routing to Waiters | 0 | 0 |
| 94.0813 | Queue | 9 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 94.3516 | Arrival | 10 | Host | enters the system | 0 | 0 |
| 94.3516 | Seize | 10 | Host | server free, service 6.0420 until 100.3936 | 0 | 1 |
| 100.3936 | Move | 10 | Host | service done, routing to Waiters | 0 | 0 |
| 100.3936 | Queue | 10 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 103.9072 | Move | 6 | Waiters | service done, routing to Cashier | 2 | 2 |
| 103.9072 | Seize | 9 | Waiters | pulled from queue after waiting 9.8259, service until 138.3728 | 1 | 3 |
| 103.9072 | Seize | 6 | Cashier | server free, service 1.8295 until 105.7368 | 0 | 1 |
| 105.7368 | Exit | 6 | Cashier | exits; total wait 22.2397, time in system 74.1062 | 0 | 0 |
| 108.6188 | Arrival | 11 | Host | enters the system | 0 | 0 |
| 108.6188 | Seize | 11 | Host | server free, service 6.3361 until 114.9549 | 0 | 1 |
| 114.9549 | Move | 11 | Host | service done, routing to Waiters | 0 | 0 |
| 114.9549 | Queue | 11 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 116.1973 | Move | 7 | Waiters | service done, routing to Cashier | 2 | 2 |
| 116.1973 | Seize | 10 | Waiters | pulled from queue after waiting 15.8037, service until 146.5774 | 1 | 3 |
| 116.1973 | Seize | 7 | Cashier | server free, service 1.4281 until 117.6255 | 0 | 1 |
| 117.4914 | Arrival | 12 | Host | enters the system | 0 | 0 |
| 117.4914 | Seize | 12 | Host | server free, service 1.5848 until 119.0762 | 0 | 1 |
| 117.6255 | Exit | 7 | Cashier | exits; total wait 30.6273, time in system 69.1668 | 0 | 0 |
| 118.7994 | Arrival | 13 | Host | enters the system | 0 | 1 |
| 118.7994 | Queue | 13 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 119.0762 | Move | 12 | Host | service done, routing to Waiters | 1 | 0 |
| 119.0762 | Seize | 13 | Host | pulled from queue after waiting 0.2767, service until 119.7407 | 0 | 1 |
| 119.0762 | Queue | 12 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 119.7407 | Move | 13 | Host | service done, routing to Waiters | 0 | 0 |
| 119.7407 | Queue | 13 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 123.4383 | Move | 8 | Waiters | service done, routing to Cashier | 3 | 2 |
| 123.4383 | Seize | 11 | Waiters | pulled from queue after waiting 8.4834, service until 174.5497 | 2 | 3 |
| 123.4383 | Seize | 8 | Cashier | server free, service 2.3000 until 125.7383 | 0 | 1 |
| 124.3699 | Arrival | 14 | Host | enters the system | 0 | 0 |
| 124.3699 | Seize | 14 | Host | server free, service 8.5695 until 132.9395 | 0 | 1 |
| 125.7383 | Exit | 8 | Cashier | exits; total wait 13.9447, time in system 56.4394 | 0 | 0 |
| 132.5087 | Arrival | 15 | Host | enters the system | 0 | 1 |
| 132.5087 | Queue | 15 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 132.9395 | Move | 14 | Host | service done, routing to Waiters | 1 | 0 |
| 132.9395 | Seize | 15 | Host | pulled from queue after waiting 0.4307, service until 133.7805 | 0 | 1 |
| 132.9395 | Queue | 14 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 133.7805 | Move | 15 | Host | service done, routing to Waiters | 0 | 0 |
| 133.7805 | Queue | 15 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 138.3728 | Move | 9 | Waiters | service done, routing to Cashier | 4 | 2 |
| 138.3728 | Seize | 12 | Waiters | pulled from queue after waiting 19.2967, service until 171.7968 | 3 | 3 |
| 138.3728 | Seize | 9 | Cashier | server free, service 2.5119 until 140.8847 | 0 | 1 |
| 140.8847 | Exit | 9 | Cashier | exits; total wait 9.8259, time in system 48.1214 | 0 | 0 |
| 146.5774 | Move | 10 | Waiters | service done, routing to Cashier | 3 | 2 |
| 146.5774 | Seize | 13 | Waiters | pulled from queue after waiting 26.8367, service until 187.1675 | 2 | 3 |
| 146.5774 | Seize | 10 | Cashier | server free, service 2.9709 until 149.5483 | 0 | 1 |
| 149.5483 | Exit | 10 | Cashier | exits; total wait 15.8037, time in system 55.1967 | 0 | 0 |
| 156.1756 | Arrival | 16 | Host | enters the system | 0 | 0 |
| 156.1756 | Seize | 16 | Host | server free, service 1.0391 until 157.2147 | 0 | 1 |
| 157.2147 | Move | 16 | Host | service done, routing to Waiters | 0 | 0 |
| 157.2147 | Queue | 16 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 160.6747 | Arrival | 17 | Host | enters the system | 0 | 0 |
| 160.6747 | Seize | 17 | Host | server free, service 1.1579 until 161.8326 | 0 | 1 |
| 161.8326 | Move | 17 | Host | service done, routing to Waiters | 0 | 0 |
| 161.8326 | Queue | 17 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 171.7968 | Move | 12 | Waiters | service done, routing to Cashier | 4 | 2 |
| 171.7968 | Seize | 14 | Waiters | pulled from queue after waiting 38.8574, service until 217.0230 | 3 | 3 |
| 171.7968 | Seize | 12 | Cashier | server free, service 3.0539 until 174.8507 | 0 | 1 |
| 174.5497 | Move | 11 | Waiters | service done, routing to Cashier | 3 | 2 |
| 174.5497 | Seize | 15 | Waiters | pulled from queue after waiting 40.7692, service until 218.3224 | 2 | 3 |
| 174.5497 | Queue | 11 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 174.8507 | Exit | 12 | Cashier | exits; total wait 19.2967, time in system 57.3593 | 1 | 0 |
| 174.8507 | Seize | 11 | Cashier | pulled from queue after waiting 0.3010, service until 177.3190 | 0 | 1 |
| 177.1806 | Arrival | 18 | Host | enters the system | 0 | 0 |
| 177.1806 | Seize | 18 | Host | server free, service 0.3339 until 177.5145 | 0 | 1 |
| 177.3190 | Exit | 11 | Cashier | exits; total wait 8.7844, time in system 68.7002 | 0 | 0 |
| 177.5145 | Move | 18 | Host | service done, routing to Waiters | 0 | 0 |
| 177.5145 | Queue | 18 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 187.1675 | Move | 13 | Waiters | service done, routing to Cashier | 3 | 2 |
| 187.1675 | Seize | 16 | Waiters | pulled from queue after waiting 29.9528, service until 228.4357 | 2 | 3 |
| 187.1675 | Seize | 13 | Cashier | server free, service 2.6722 until 189.8397 | 0 | 1 |
| 189.8397 | Exit | 13 | Cashier | exits; total wait 27.1134, time in system 71.0403 | 0 | 0 |
| 217.0230 | Move | 14 | Waiters | service done, routing to Cashier | 2 | 2 |
| 217.0230 | Seize | 17 | Waiters | pulled from queue after waiting 55.1905, service until 265.5571 | 1 | 3 |
| 217.0230 | Seize | 14 | Cashier | server free, service 1.4309 until 218.4539 | 0 | 1 |
| 218.3224 | Move | 15 | Waiters | service done, routing to Cashier | 1 | 2 |
| 218.3224 | Seize | 18 | Waiters | pulled from queue after waiting 40.8079, service until 254.8257 | 0 | 3 |
| 218.3224 | Queue | 15 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 218.4539 | Exit | 14 | Cashier | exits; total wait 38.8574, time in system 94.0840 | 1 | 0 |
| 218.4539 | Seize | 15 | Cashier | pulled from queue after waiting 0.1315, service until 221.4316 | 0 | 1 |
| 221.4316 | Exit | 15 | Cashier | exits; total wait 41.3314, time in system 88.9229 | 0 | 0 |
| 223.3646 | Arrival | 19 | Host | enters the system | 0 | 0 |
| 223.3646 | Seize | 19 | Host | server free, service 3.6473 until 227.0119 | 0 | 1 |
| 227.0119 | Move | 19 | Host | service done, routing to Waiters | 0 | 0 |
| 227.0119 | Queue | 19 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 228.4357 | Move | 16 | Waiters | service done, routing to Cashier | 1 | 2 |
| 228.4357 | Seize | 19 | Waiters | pulled from queue after waiting 1.4238, service until 266.8153 | 0 | 3 |
| 228.4357 | Seize | 16 | Cashier | server free, service 1.1189 until 229.5546 | 0 | 1 |
| 229.5546 | Exit | 16 | Cashier | exits; total wait 29.9528, time in system 73.3790 | 0 | 0 |
| 239.6223 | Arrival | 20 | Host | enters the system | 0 | 0 |
| 239.6223 | Seize | 20 | Host | server free, service 3.2079 until 242.8302 | 0 | 1 |
| 241.1295 | Arrival | 21 | Host | enters the system | 0 | 1 |
| 241.1295 | Queue | 21 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 242.8302 | Move | 20 | Host | service done, routing to Waiters | 1 | 0 |
| 242.8302 | Seize | 21 | Host | pulled from queue after waiting 1.7006, service until 245.1244 | 0 | 1 |
| 242.8302 | Queue | 20 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 245.1244 | Move | 21 | Host | service done, routing to Waiters | 0 | 0 |
| 245.1244 | Queue | 21 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 248.8518 | Arrival | 22 | Host | enters the system | 0 | 0 |
| 248.8518 | Seize | 22 | Host | server free, service 1.0123 until 249.8640 | 0 | 1 |
| 249.8640 | Move | 22 | Host | service done, routing to Waiters | 0 | 0 |
| 249.8640 | Queue | 22 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 254.8257 | Move | 18 | Waiters | service done, routing to Cashier | 3 | 2 |
| 254.8257 | Seize | 20 | Waiters | pulled from queue after waiting 11.9955, service until 300.1907 | 2 | 3 |
| 254.8257 | Seize | 18 | Cashier | server free, service 3.8540 until 258.6797 | 0 | 1 |
| 258.6797 | Exit | 18 | Cashier | exits; total wait 40.8079, time in system 81.4991 | 0 | 0 |
| 259.3068 | Arrival | 23 | Host | enters the system | 0 | 0 |
| 259.3068 | Seize | 23 | Host | server free, service 1.4674 until 260.7742 | 0 | 1 |
| 260.7742 | Move | 23 | Host | service done, routing to Waiters | 0 | 0 |
| 260.7742 | Queue | 23 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 263.7820 | Arrival | 24 | Host | enters the system | 0 | 0 |
| 263.7820 | Seize | 24 | Host | server free, service 1.2300 until 265.0120 | 0 | 1 |
| 265.0120 | Move | 24 | Host | service done, routing to Waiters | 0 | 0 |
| 265.0120 | Queue | 24 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 265.5571 | Move | 17 | Waiters | service done, routing to Cashier | 4 | 2 |
| 265.5571 | Seize | 21 | Waiters | pulled from queue after waiting 20.4326, service until 293.6813 | 3 | 3 |
| 265.5571 | Seize | 17 | Cashier | server free, service 3.3970 until 268.9541 | 0 | 1 |
| 266.8153 | Move | 19 | Waiters | service done, routing to Cashier | 3 | 2 |
| 266.8153 | Seize | 22 | Waiters | pulled from queue after waiting 16.9512, service until 304.4081 | 2 | 3 |
| 266.8153 | Queue | 19 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 268.9541 | Exit | 17 | Cashier | exits; total wait 55.1905, time in system 108.2794 | 1 | 0 |
| 268.9541 | Seize | 19 | Cashier | pulled from queue after waiting 2.1388, service until 270.5456 | 0 | 1 |
| 270.5456 | Exit | 19 | Cashier | exits; total wait 3.5626, time in system 47.1811 | 0 | 0 |
| 293.6813 | Move | 21 | Waiters | service done, routing to Cashier | 2 | 2 |
| 293.6813 | Seize | 23 | Waiters | pulled from queue after waiting 32.9071, service until 342.5492 | 1 | 3 |
| 293.6813 | Seize | 21 | Cashier | server free, service 1.8045 until 295.4858 | 0 | 1 |
| 294.1680 | Arrival | 25 | Host | enters the system | 0 | 0 |
| 294.1680 | Seize | 25 | Host | server free, service 0.9323 until 295.1003 | 0 | 1 |
| 295.1003 | Move | 25 | Host | service done, routing to Waiters | 0 | 0 |
| 295.1003 | Queue | 25 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 295.4858 | Exit | 21 | Cashier | exits; total wait 22.1333, time in system 54.3563 | 0 | 0 |
| 298.2243 | Arrival | 26 | Host | enters the system | 0 | 0 |
| 298.2243 | Seize | 26 | Host | server free, service 0.5308 until 298.7550 | 0 | 1 |
| 298.7550 | Move | 26 | Host | service done, routing to Waiters | 0 | 0 |
| 298.7550 | Queue | 26 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 300.1907 | Move | 20 | Waiters | service done, routing to Cashier | 3 | 2 |
| 300.1907 | Seize | 24 | Waiters | pulled from queue after waiting 35.1787, service until 330.4604 | 2 | 3 |
| 300.1907 | Seize | 20 | Cashier | server free, service 2.1955 until 302.3862 | 0 | 1 |
| 302.3862 | Exit | 20 | Cashier | exits; total wait 11.9955, time in system 62.7639 | 0 | 0 |
| 304.4081 | Move | 22 | Waiters | service done, routing to Cashier | 2 | 2 |
| 304.4081 | Seize | 25 | Waiters | pulled from queue after waiting 9.3077, service until 351.8236 | 1 | 3 |
| 304.4081 | Seize | 22 | Cashier | server free, service 3.3602 until 307.7683 | 0 | 1 |
| 305.5383 | Arrival | 27 | Host | enters the system | 0 | 0 |
| 305.5383 | Seize | 27 | Host | server free, service 1.3704 until 306.9088 | 0 | 1 |
| 306.9088 | Move | 27 | Host | service done, routing to Waiters | 0 | 0 |
| 306.9088 | Queue | 27 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 307.7683 | Exit | 22 | Cashier | exits; total wait 16.9512, time in system 58.9165 | 0 | 0 |
| 320.8957 | Arrival | 28 | Host | enters the system | 0 | 0 |
| 320.8957 | Seize | 28 | Host | server free, service 1.2858 until 322.1815 | 0 | 1 |
| 322.1815 | Move | 28 | Host | service done, routing to Waiters | 0 | 0 |
| 322.1815 | Queue | 28 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 330.4604 | Move | 24 | Waiters | service done, routing to Cashier | 3 | 2 |
| 330.4604 | Seize | 26 | Waiters | pulled from queue after waiting 31.7054, service until 382.9489 | 2 | 3 |
| 330.4604 | Seize | 24 | Cashier | server free, service 3.2134 until 333.6738 | 0 | 1 |
| 330.5056 | Arrival | 29 | Host | enters the system | 0 | 0 |
| 330.5056 | Seize | 29 | Host | server free, service 2.8358 until 333.3414 | 0 | 1 |
| 331.1425 | Arrival | 30 | Host | enters the system | 0 | 1 |
| 331.1425 | Queue | 30 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 333.3414 | Move | 29 | Host | service done, routing to Waiters | 1 | 0 |
| 333.3414 | Seize | 30 | Host | pulled from queue after waiting 2.1989, service until 333.7345 | 0 | 1 |
| 333.3414 | Queue | 29 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 333.6738 | Exit | 24 | Cashier | exits; total wait 35.1787, time in system 69.8919 | 0 | 0 |
| 333.7345 | Move | 30 | Host | service done, routing to Waiters | 0 | 0 |
| 333.7345 | Queue | 30 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 341.9472 | Arrival | 31 | Host | enters the system | 0 | 0 |
| 341.9472 | Seize | 31 | Host | server free, service 5.6133 until 347.5605 | 0 | 1 |
| 342.5492 | Move | 23 | Waiters | service done, routing to Cashier | 4 | 2 |
| 342.5492 | Seize | 27 | Waiters | pulled from queue after waiting 35.6404, service until 373.8519 | 3 | 3 |
| 342.5492 | Seize | 23 | Cashier | server free, service 3.4399 until 345.9891 | 0 | 1 |
| 345.9891 | Exit | 23 | Cashier | exits; total wait 32.9071, time in system 86.6823 | 0 | 0 |
| 347.5605 | Move | 31 | Host | service done, routing to Waiters | 0 | 0 |
| 347.5605 | Queue | 31 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 349.4110 | Arrival | 32 | Host | enters the system | 0 | 0 |
| 349.4110 | Seize | 32 | Host | server free, service 1.7673 until 351.1782 | 0 | 1 |
| 351.1782 | Move | 32 | Host | service done, routing to Waiters | 0 | 0 |
| 351.1782 | Queue | 32 | Waiters | all 3 busy, queued at position 5 | 5 | 3 |
| 351.8236 | Move | 25 | Waiters | service done, routing to Cashier | 5 | 2 |
| 351.8236 | Seize | 28 | Waiters | pulled from queue after waiting 29.6421, service until 382.3876 | 4 | 3 |
| 351.8236 | Seize | 25 | Cashier | server free, service 3.4370 until 355.2605 | 0 | 1 |
| 355.2605 | Exit | 25 | Cashier | exits; total wait 9.3077, time in system 61.0925 | 0 | 0 |
| 357.1649 | Arrival | 33 | Host | enters the system | 0 | 0 |
| 357.1649 | Seize | 33 | Host | server free, service 1.1846 until 358.3494 | 0 | 1 |
| 358.3494 | Move | 33 | Host | service done, routing to Waiters | 0 | 0 |
| 358.3494 | Queue | 33 | Waiters | all 3 busy, queued at position 5 | 5 | 3 |
| 370.2077 | Arrival | 34 | Host | enters the system | 0 | 0 |
| 370.2077 | Seize | 34 | Host | server free, service 1.5717 until 371.7794 | 0 | 1 |
| 371.7794 | Move | 34 | Host | service done, routing to Waiters | 0 | 0 |
| 371.7794 | Queue | 34 | Waiters | all 3 busy, queued at position 6 | 6 | 3 |
| 373.8519 | Move | 27 | Waiters | service done, routing to Cashier | 6 | 2 |
| 373.8519 | Seize | 29 | Waiters | pulled from queue after waiting 40.5105, service until 409.7358 | 5 | 3 |
| 373.8519 | Seize | 27 | Cashier | server free, service 1.0575 until 374.9093 | 0 | 1 |
| 374.9093 | Exit | 27 | Cashier | exits; total wait 35.6404, time in system 69.3710 | 0 | 0 |
| 382.3876 | Move | 28 | Waiters | service done, routing to Cashier | 5 | 2 |
| 382.3876 | Seize | 30 | Waiters | pulled from queue after waiting 48.6532, service until 424.6049 | 4 | 3 |
| 382.3876 | Seize | 28 | Cashier | server free, service 2.4643 until 384.8519 | 0 | 1 |
| 382.9489 | Move | 26 | Waiters | service done, routing to Cashier | 4 | 2 |
| 382.9489 | Seize | 31 | Waiters | pulled from queue after waiting 35.3884, service until 414.4532 | 3 | 3 |
| 382.9489 | Queue | 26 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 384.8519 | Exit | 28 | Cashier | exits; total wait 29.6421, time in system 63.9562 | 1 | 0 |
| 384.8519 | Seize | 26 | Cashier | pulled from queue after waiting 1.9030, service until 387.5035 | 0 | 1 |
| 387.5035 | Exit | 26 | Cashier | exits; total wait 33.6084, time in system 89.2793 | 0 | 0 |
| 394.2060 | Arrival | 35 | Host | enters the system | 0 | 0 |
| 394.2060 | Seize | 35 | Host | server free, service 1.3371 until 395.5431 | 0 | 1 |
| 395.5431 | Move | 35 | Host | service done, routing to Waiters | 0 | 0 |
| 395.5431 | Queue | 35 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 405.2968 | Arrival | 36 | Host | enters the system | 0 | 0 |
| 405.2968 | Seize | 36 | Host | server free, service 1.4985 until 406.7953 | 0 | 1 |
| 406.7953 | Move | 36 | Host | service done, routing to Waiters | 0 | 0 |
| 406.7953 | Queue | 36 | Waiters | all 3 busy, queued at position 5 | 5 | 3 |
| 409.7358 | Move | 29 | Waiters | service done, routing to Cashier | 5 | 2 |
| 409.7358 | Seize | 32 | Waiters | pulled from queue after waiting 58.5576, service until 465.7945 | 4 | 3 |
| 409.7358 | Seize | 29 | Cashier | server free, service 2.8703 until 412.6061 | 0 | 1 |
| 409.8749 | Arrival | 37 | Host | enters the system | 0 | 0 |
| 409.8749 | Seize | 37 | Host | server free, service 4.8153 until 414.6902 | 0 | 1 |
| 412.6061 | Exit | 29 | Cashier | exits; total wait 40.5105, time in system 82.1005 | 0 | 0 |
| 414.4532 | Move | 31 | Waiters | service done, routing to Cashier | 4 | 2 |
| 414.4532 | Seize | 33 | Waiters | pulled from queue after waiting 56.1038, service until 468.3311 | 3 | 3 |
| 414.4532 | Seize | 31 | Cashier | server free, service 2.7553 until 417.2085 | 0 | 1 |
| 414.6902 | Move | 37 | Host | service done, routing to Waiters | 0 | 0 |
| 414.6902 | Queue | 37 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 417.2085 | Exit | 31 | Cashier | exits; total wait 35.3884, time in system 75.2613 | 0 | 0 |
| 424.6049 | Move | 30 | Waiters | service done, routing to Cashier | 4 | 2 |
| 424.6049 | Seize | 34 | Waiters | pulled from queue after waiting 52.8255, service until 454.5517 | 3 | 3 |
| 424.6049 | Seize | 30 | Cashier | server free, service 2.1748 until 426.7796 | 0 | 1 |
| 424.7707 | Arrival | 38 | Host | enters the system | 0 | 0 |
| 424.7707 | Seize | 38 | Host | server free, service 0.8413 until 425.6121 | 0 | 1 |
| 425.6121 | Move | 38 | Host | service done, routing to Waiters | 0 | 0 |
| 425.6121 | Queue | 38 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 426.7796 | Exit | 30 | Cashier | exits; total wait 50.8520, time in system 95.6371 | 0 | 0 |
| 454.5517 | Move | 34 | Waiters | service done, routing to Cashier | 4 | 2 |
| 454.5517 | Seize | 35 | Waiters | pulled from queue after waiting 59.0085, service until 497.3781 | 3 | 3 |
| 454.5517 | Seize | 34 | Cashier | server free, service 2.3887 until 456.9403 | 0 | 1 |
| 456.9403 | Exit | 34 | Cashier | exits; total wait 52.8255, time in system 86.7327 | 0 | 0 |
| 465.7945 | Move | 32 | Waiters | service done, routing to Cashier | 3 | 2 |
| 465.7945 | Seize | 36 | Waiters | pulled from queue after waiting 58.9992, service until 501.0314 | 2 | 3 |
| 465.7945 | Seize | 32 | Cashier | server free, service 2.5853 until 468.3798 | 0 | 1 |
| 468.1562 | Arrival | 39 | Host | enters the system | 0 | 0 |
| 468.1562 | Seize | 39 | Host | server free, service 2.0349 until 470.1910 | 0 | 1 |
| 468.3311 | Move | 33 | Waiters | service done, routing to Cashier | 2 | 2 |
| 468.3311 | Seize | 37 | Waiters | pulled from queue after waiting 53.6409, service until 516.6216 | 1 | 3 |
| 468.3311 | Queue | 33 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 468.3798 | Exit | 32 | Cashier | exits; total wait 58.5576, time in system 118.9688 | 1 | 0 |
| 468.3798 | Seize | 33 | Cashier | pulled from queue after waiting 0.0487, service until 471.2897 | 0 | 1 |
| 470.1910 | Move | 39 | Host | service done, routing to Waiters | 0 | 0 |
| 470.1910 | Queue | 39 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 471.2897 | Exit | 33 | Cashier | exits; total wait 56.1525, time in system 114.1248 | 0 | 0 |
| 494.0196 | Arrival | 40 | Host | enters the system | 0 | 0 |
| 494.0196 | Seize | 40 | Host | server free, service 1.4587 until 495.4784 | 0 | 1 |
| 495.4784 | Move | 40 | Host | service done, routing to Waiters | 0 | 0 |
| 495.4784 | Queue | 40 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 497.3781 | Move | 35 | Waiters | service done, routing to Cashier | 3 | 2 |
| 497.3781 | Seize | 38 | Waiters | pulled from queue after waiting 71.7661, service until 531.3985 | 2 | 3 |
| 497.3781 | Seize | 35 | Cashier | server free, service 1.2963 until 498.6745 | 0 | 1 |
| 498.6745 | Exit | 35 | Cashier | exits; total wait 59.0085, time in system 104.4684 | 0 | 0 |
| 501.0314 | Move | 36 | Waiters | service done, routing to Cashier | 2 | 2 |
| 501.0314 | Seize | 39 | Waiters | pulled from queue after waiting 30.8404, service until 540.2171 | 1 | 3 |
| 501.0314 | Seize | 36 | Cashier | server free, service 1.6456 until 502.6770 | 0 | 1 |
| 502.6770 | Exit | 36 | Cashier | exits; total wait 58.9992, time in system 97.3803 | 0 | 0 |
| 516.6216 | Move | 37 | Waiters | service done, routing to Cashier | 1 | 2 |
| 516.6216 | Seize | 40 | Waiters | pulled from queue after waiting 21.1433, service until 539.2814 | 0 | 3 |
| 516.6216 | Seize | 37 | Cashier | server free, service 1.8976 until 518.5192 | 0 | 1 |
| 518.5192 | Exit | 37 | Cashier | exits; total wait 53.6409, time in system 108.6443 | 0 | 0 |
| 531.3985 | Move | 38 | Waiters | service done, routing to Cashier | 0 | 2 |
| 531.3985 | Seize | 38 | Cashier | server free, service 1.1169 until 532.5154 | 0 | 1 |
| 532.5154 | Exit | 38 | Cashier | exits; total wait 71.7661, time in system 107.7447 | 0 | 0 |
| 539.2814 | Move | 40 | Waiters | service done, routing to Cashier | 0 | 1 |
| 539.2814 | Seize | 40 | Cashier | server free, service 3.9286 until 543.2100 | 0 | 1 |
| 540.2171 | Move | 39 | Waiters | service done, routing to Cashier | 0 | 0 |
| 540.2171 | Queue | 39 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 543.2100 | Exit | 40 | Cashier | exits; total wait 21.1433, time in system 49.1904 | 1 | 0 |
| 543.2100 | Seize | 39 | Cashier | pulled from queue after waiting 2.9929, service until 545.8644 | 0 | 1 |
| 545.8644 | Exit | 39 | Cashier | exits; total wait 33.8333, time in system 77.7082 | 0 | 0 |
| 600.0000 | WarmUpEnd | 0 | - | statistics discarded; measurement starts here | 0 | 0 |
| 602.9233 | Arrival | 41 | Host | enters the system | 0 | 0 |
| 602.9233 | Seize | 41 | Host | server free, service 0.8312 until 603.7545 | 0 | 1 |
| 603.7545 | Move | 41 | Host | service done, routing to Waiters | 0 | 0 |
| 603.7545 | Seize | 41 | Waiters | server free, service 32.0723 until 635.8269 | 0 | 1 |
| 604.0396 | Arrival | 42 | Host | enters the system | 0 | 0 |
| 604.0396 | Seize | 42 | Host | server free, service 2.9422 until 606.9818 | 0 | 1 |
| 605.7430 | Arrival | 43 | Host | enters the system | 0 | 1 |
| 605.7430 | Queue | 43 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 606.9818 | Move | 42 | Host | service done, routing to Waiters | 1 | 0 |
| 606.9818 | Seize | 43 | Host | pulled from queue after waiting 1.2388, service until 607.3974 | 0 | 1 |
| 606.9818 | Seize | 42 | Waiters | server free, service 50.6540 until 657.6358 | 0 | 2 |
| 607.3974 | Move | 43 | Host | service done, routing to Waiters | 0 | 0 |
| 607.3974 | Seize | 43 | Waiters | server free, service 48.2165 until 655.6139 | 0 | 3 |
| 612.2186 | Arrival | 44 | Host | enters the system | 0 | 0 |
| 612.2186 | Seize | 44 | Host | server free, service 1.5808 until 613.7994 | 0 | 1 |
| 613.7994 | Move | 44 | Host | service done, routing to Waiters | 0 | 0 |
| 613.7994 | Queue | 44 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 619.8535 | Arrival | 45 | Host | enters the system | 0 | 0 |
| 619.8535 | Seize | 45 | Host | server free, service 2.3657 until 622.2192 | 0 | 1 |
| 622.2192 | Move | 45 | Host | service done, routing to Waiters | 0 | 0 |
| 622.2192 | Queue | 45 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 628.8313 | Arrival | 46 | Host | enters the system | 0 | 0 |
| 628.8313 | Seize | 46 | Host | server free, service 0.7825 until 629.6138 | 0 | 1 |
| 629.6138 | Move | 46 | Host | service done, routing to Waiters | 0 | 0 |
| 629.6138 | Queue | 46 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 632.4403 | Arrival | 47 | Host | enters the system | 0 | 0 |
| 632.4403 | Seize | 47 | Host | server free, service 0.4032 until 632.8435 | 0 | 1 |
| 632.8435 | Move | 47 | Host | service done, routing to Waiters | 0 | 0 |
| 632.8435 | Queue | 47 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 635.8269 | Move | 41 | Waiters | service done, routing to Cashier | 4 | 2 |
| 635.8269 | Seize | 44 | Waiters | pulled from queue after waiting 22.0275, service until 678.3919 | 3 | 3 |
| 635.8269 | Seize | 41 | Cashier | server free, service 2.5802 until 638.4071 | 0 | 1 |
| 638.4071 | Exit | 41 | Cashier | exits; total wait 0.0000, time in system 35.4838 | 0 | 0 |
| 652.6403 | Arrival | 48 | Host | enters the system | 0 | 0 |
| 652.6403 | Seize | 48 | Host | server free, service 2.1023 until 654.7426 | 0 | 1 |
| 654.7426 | Move | 48 | Host | service done, routing to Waiters | 0 | 0 |
| 654.7426 | Queue | 48 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 655.6139 | Move | 43 | Waiters | service done, routing to Cashier | 4 | 2 |
| 655.6139 | Seize | 45 | Waiters | pulled from queue after waiting 33.3947, service until 693.4729 | 3 | 3 |
| 655.6139 | Seize | 43 | Cashier | server free, service 3.0671 until 658.6810 | 0 | 1 |
| 655.9592 | Arrival | 49 | Host | enters the system | 0 | 0 |
| 655.9592 | Seize | 49 | Host | server free, service 2.9404 until 658.8995 | 0 | 1 |
| 657.6358 | Move | 42 | Waiters | service done, routing to Cashier | 3 | 2 |
| 657.6358 | Seize | 46 | Waiters | pulled from queue after waiting 28.0220, service until 692.9994 | 2 | 3 |
| 657.6358 | Queue | 42 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 658.6810 | Exit | 43 | Cashier | exits; total wait 1.2388, time in system 52.9380 | 1 | 0 |
| 658.6810 | Seize | 42 | Cashier | pulled from queue after waiting 1.0452, service until 662.5568 | 0 | 1 |
| 658.8995 | Move | 49 | Host | service done, routing to Waiters | 0 | 0 |
| 658.8995 | Queue | 49 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 662.5568 | Exit | 42 | Cashier | exits; total wait 1.0452, time in system 58.5172 | 0 | 0 |
| 668.7388 | Arrival | 50 | Host | enters the system | 0 | 0 |
| 668.7388 | Seize | 50 | Host | server free, service 0.4636 until 669.2023 | 0 | 1 |
| 669.2023 | Move | 50 | Host | service done, routing to Waiters | 0 | 0 |
| 669.2023 | Queue | 50 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 678.3919 | Move | 44 | Waiters | service done, routing to Cashier | 4 | 2 |
| 678.3919 | Seize | 47 | Waiters | pulled from queue after waiting 45.5484, service until 715.5009 | 3 | 3 |
| 678.3919 | Seize | 44 | Cashier | server free, service 3.3705 until 681.7624 | 0 | 1 |
| 681.7624 | Exit | 44 | Cashier | exits; total wait 22.0275, time in system 69.5438 | 0 | 0 |
| 692.9994 | Move | 46 | Waiters | service done, routing to Cashier | 3 | 2 |
| 692.9994 | Seize | 48 | Waiters | pulled from queue after waiting 38.2569, service until 721.7608 | 2 | 3 |
| 692.9994 | Seize | 46 | Cashier | server free, service 3.4810 until 696.4804 | 0 | 1 |
| 693.4729 | Move | 45 | Waiters | service done, routing to Cashier | 2 | 2 |
| 693.4729 | Seize | 49 | Waiters | pulled from queue after waiting 34.5734, service until 725.4086 | 1 | 3 |
| 693.4729 | Queue | 45 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 696.4804 | Exit | 46 | Cashier | exits; total wait 28.0220, time in system 67.6491 | 1 | 0 |
| 696.4804 | Seize | 45 | Cashier | pulled from queue after waiting 3.0075, service until 700.3021 | 0 | 1 |
| 700.3021 | Exit | 45 | Cashier | exits; total wait 36.4022, time in system 80.4486 | 0 | 0 |
| 715.5009 | Move | 47 | Waiters | service done, routing to Cashier | 1 | 2 |
| 715.5009 | Seize | 50 | Waiters | pulled from queue after waiting 46.2986, service until 762.1090 | 0 | 3 |
| 715.5009 | Seize | 47 | Cashier | server free, service 1.1553 until 716.6562 | 0 | 1 |
| 716.6562 | Exit | 47 | Cashier | exits; total wait 45.5484, time in system 84.2158 | 0 | 0 |
| 721.7608 | Move | 48 | Waiters | service done, routing to Cashier | 0 | 2 |
| 721.7608 | Seize | 48 | Cashier | server free, service 1.0056 until 722.7664 | 0 | 1 |
| 722.7664 | Exit | 48 | Cashier | exits; total wait 38.2569, time in system 70.1261 | 0 | 0 |
| 725.4086 | Move | 49 | Waiters | service done, routing to Cashier | 0 | 1 |
| 725.4086 | Seize | 49 | Cashier | server free, service 1.5309 until 726.9395 | 0 | 1 |
| 726.0249 | Arrival | 51 | Host | enters the system | 0 | 0 |
| 726.0249 | Seize | 51 | Host | server free, service 1.5445 until 727.5693 | 0 | 1 |
| 726.9395 | Exit | 49 | Cashier | exits; total wait 34.5734, time in system 70.9804 | 0 | 0 |
| 727.5693 | Move | 51 | Host | service done, routing to Waiters | 0 | 0 |
| 727.5693 | Seize | 51 | Waiters | server free, service 37.9041 until 765.4734 | 0 | 2 |
| 737.6215 | Arrival | 52 | Host | enters the system | 0 | 0 |
| 737.6215 | Seize | 52 | Host | server free, service 0.0124 until 737.6339 | 0 | 1 |
| 737.6339 | Move | 52 | Host | service done, routing to Waiters | 0 | 0 |
| 737.6339 | Seize | 52 | Waiters | server free, service 43.9148 until 781.5488 | 0 | 3 |
| 741.7813 | Arrival | 53 | Host | enters the system | 0 | 0 |
| 741.7813 | Seize | 53 | Host | server free, service 4.1543 until 745.9356 | 0 | 1 |
| 745.9356 | Move | 53 | Host | service done, routing to Waiters | 0 | 0 |
| 745.9356 | Queue | 53 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 750.7408 | Arrival | 54 | Host | enters the system | 0 | 0 |
| 750.7408 | Seize | 54 | Host | server free, service 0.0083 until 750.7491 | 0 | 1 |
| 750.7491 | Move | 54 | Host | service done, routing to Waiters | 0 | 0 |
| 750.7491 | Queue | 54 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 758.0806 | Arrival | 55 | Host | enters the system | 0 | 0 |
| 758.0806 | Seize | 55 | Host | server free, service 0.0747 until 758.1553 | 0 | 1 |
| 758.1553 | Move | 55 | Host | service done, routing to Waiters | 0 | 0 |
| 758.1553 | Queue | 55 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 760.4604 | Arrival | 56 | Host | enters the system | 0 | 0 |
| 760.4604 | Seize | 56 | Host | server free, service 0.6093 until 761.0697 | 0 | 1 |
| 761.0697 | Move | 56 | Host | service done, routing to Waiters | 0 | 0 |
| 761.0697 | Queue | 56 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 762.1090 | Move | 50 | Waiters | service done, routing to Cashier | 4 | 2 |
| 762.1090 | Seize | 53 | Waiters | pulled from queue after waiting 16.1734, service until 802.3416 | 3 | 3 |
| 762.1090 | Seize | 50 | Cashier | server free, service 1.4449 until 763.5539 | 0 | 1 |
| 763.5539 | Exit | 50 | Cashier | exits; total wait 46.2986, time in system 94.8152 | 0 | 0 |
| 765.2263 | Arrival | 57 | Host | enters the system | 0 | 0 |
| 765.2263 | Seize | 57 | Host | server free, service 2.5551 until 767.7813 | 0 | 1 |
| 765.4734 | Move | 51 | Waiters | service done, routing to Cashier | 3 | 2 |
| 765.4734 | Seize | 54 | Waiters | pulled from queue after waiting 14.7242, service until 798.1443 | 2 | 3 |
| 765.4734 | Seize | 51 | Cashier | server free, service 1.5547 until 767.0281 | 0 | 1 |
| 767.0281 | Exit | 51 | Cashier | exits; total wait 0.0000, time in system 41.0032 | 0 | 0 |
| 767.7813 | Move | 57 | Host | service done, routing to Waiters | 0 | 0 |
| 767.7813 | Queue | 57 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 768.7184 | Arrival | 58 | Host | enters the system | 0 | 0 |
| 768.7184 | Seize | 58 | Host | server free, service 0.1909 until 768.9093 | 0 | 1 |
| 768.9093 | Move | 58 | Host | service done, routing to Waiters | 0 | 0 |
| 768.9093 | Queue | 58 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 781.5488 | Move | 52 | Waiters | service done, routing to Cashier | 4 | 2 |
| 781.5488 | Seize | 55 | Waiters | pulled from queue after waiting 23.3935, service until 810.3693 | 3 | 3 |
| 781.5488 | Seize | 52 | Cashier | server free, service 3.0388 until 784.5875 | 0 | 1 |
| 784.5875 | Exit | 52 | Cashier | exits; total wait 0.0000, time in system 46.9660 | 0 | 0 |
| 793.6576 | Arrival | 59 | Host | enters the system | 0 | 0 |
| 793.6576 | Seize | 59 | Host | server free, service 0.9875 until 794.6451 | 0 | 1 |
| 794.6451 | Move | 59 | Host | service done, routing to Waiters | 0 | 0 |
| 794.6451 | Queue | 59 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 798.1443 | Move | 54 | Waiters | service done, routing to Cashier | 4 | 2 |
| 798.1443 | Seize | 56 | Waiters | pulled from queue after waiting 37.0746, service until 839.7553 | 3 | 3 |
| 798.1443 | Seize | 54 | Cashier | server free, service 1.0315 until 799.1758 | 0 | 1 |
| 799.1758 | Exit | 54 | Cashier | exits; total wait 14.7242, time in system 48.4350 | 0 | 0 |
| 802.3416 | Move | 53 | Waiters | service done, routing to Cashier | 3 | 2 |
| 802.3416 | Seize | 57 | Waiters | pulled from queue after waiting 34.5602, service until 834.8680 | 2 | 3 |
| 802.3416 | Seize | 53 | Cashier | server free, service 3.2567 until 805.5983 | 0 | 1 |
| 802.6141 | Arrival | 60 | Host | enters the system | 0 | 0 |
| 802.6141 | Seize | 60 | Host | server free, service 2.2521 until 804.8662 | 0 | 1 |
| 804.8662 | Move | 60 | Host | service done, routing to Waiters | 0 | 0 |
| 804.8662 | Queue | 60 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 805.5983 | Exit | 53 | Cashier | exits; total wait 16.1734, time in system 63.8170 | 0 | 0 |
| 810.3693 | Move | 55 | Waiters | service done, routing to Cashier | 3 | 2 |
| 810.3693 | Seize | 58 | Waiters | pulled from queue after waiting 41.4600, service until 847.9745 | 2 | 3 |
| 810.3693 | Seize | 55 | Cashier | server free, service 3.3260 until 813.6953 | 0 | 1 |
| 813.6953 | Exit | 55 | Cashier | exits; total wait 23.3935, time in system 55.6147 | 0 | 0 |
| 834.8680 | Move | 57 | Waiters | service done, routing to Cashier | 2 | 2 |
| 834.8680 | Seize | 59 | Waiters | pulled from queue after waiting 40.2229, service until 874.0126 | 1 | 3 |
| 834.8680 | Seize | 57 | Cashier | server free, service 1.9115 until 836.7795 | 0 | 1 |
| 836.7795 | Exit | 57 | Cashier | exits; total wait 34.5602, time in system 71.5532 | 0 | 0 |
| 839.7553 | Move | 56 | Waiters | service done, routing to Cashier | 1 | 2 |
| 839.7553 | Seize | 60 | Waiters | pulled from queue after waiting 34.8891, service until 876.9721 | 0 | 3 |
| 839.7553 | Seize | 56 | Cashier | server free, service 1.9451 until 841.7004 | 0 | 1 |
| 841.7004 | Exit | 56 | Cashier | exits; total wait 37.0746, time in system 81.2400 | 0 | 0 |
| 844.2002 | Arrival | 61 | Host | enters the system | 0 | 0 |
| 844.2002 | Seize | 61 | Host | server free, service 1.6831 until 845.8832 | 0 | 1 |
| 844.7816 | Arrival | 62 | Host | enters the system | 0 | 1 |
| 844.7816 | Queue | 62 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 845.8832 | Move | 61 | Host | service done, routing to Waiters | 1 | 0 |
| 845.8832 | Seize | 62 | Host | pulled from queue after waiting 1.1016, service until 846.8176 | 0 | 1 |
| 845.8832 | Queue | 61 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 846.8176 | Move | 62 | Host | service done, routing to Waiters | 0 | 0 |
| 846.8176 | Queue | 62 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 847.9745 | Move | 58 | Waiters | service done, routing to Cashier | 2 | 2 |
| 847.9745 | Seize | 61 | Waiters | pulled from queue after waiting 2.0912, service until 890.0794 | 1 | 3 |
| 847.9745 | Seize | 58 | Cashier | server free, service 1.3663 until 849.3407 | 0 | 1 |
| 849.3407 | Exit | 58 | Cashier | exits; total wait 41.4600, time in system 80.6223 | 0 | 0 |
| 874.0126 | Move | 59 | Waiters | service done, routing to Cashier | 1 | 2 |
| 874.0126 | Seize | 62 | Waiters | pulled from queue after waiting 27.1950, service until 901.4685 | 0 | 3 |
| 874.0126 | Seize | 59 | Cashier | server free, service 2.8989 until 876.9114 | 0 | 1 |
| 876.9114 | Exit | 59 | Cashier | exits; total wait 40.2229, time in system 83.2539 | 0 | 0 |
| 876.9721 | Move | 60 | Waiters | service done, routing to Cashier | 0 | 2 |
| 876.9721 | Seize | 60 | Cashier | server free, service 1.8426 until 878.8148 | 0 | 1 |
| 878.8148 | Exit | 60 | Cashier | exits; total wait 34.8891, time in system 76.2006 | 0 | 0 |
| 890.0794 | Move | 61 | Waiters | service done, routing to Cashier | 0 | 1 |
| 890.0794 | Seize | 61 | Cashier | server free, service 1.8142 until 891.8937 | 0 | 1 |
| 891.8937 | Exit | 61 | Cashier | exits; total wait 2.0912, time in system 47.6935 | 0 | 0 |
| 901.4685 | Move | 62 | Waiters | service done, routing to Cashier | 0 | 0 |
| 901.4685 | Seize | 62 | Cashier | server free, service 3.6707 until 905.1392 | 0 | 1 |
| 905.1392 | Exit | 62 | Cashier | exits; total wait 28.2966, time in system 60.3575 | 0 | 0 |
| 908.5615 | Arrival | 63 | Host | enters the system | 0 | 0 |
| 908.5615 | Seize | 63 | Host | server free, service 0.6881 until 909.2496 | 0 | 1 |
| 909.2496 | Move | 63 | Host | service done, routing to Waiters | 0 | 0 |
| 909.2496 | Seize | 63 | Waiters | server free, service 38.7216 until 947.9712 | 0 | 1 |
| 911.9746 | Arrival | 64 | Host | enters the system | 0 | 0 |
| 911.9746 | Seize | 64 | Host | server free, service 1.4611 until 913.4357 | 0 | 1 |
| 913.4357 | Move | 64 | Host | service done, routing to Waiters | 0 | 0 |
| 913.4357 | Seize | 64 | Waiters | server free, service 43.6777 until 957.1133 | 0 | 2 |
| 930.8716 | Arrival | 65 | Host | enters the system | 0 | 0 |
| 930.8716 | Seize | 65 | Host | server free, service 1.7649 until 932.6365 | 0 | 1 |
| 931.7834 | Arrival | 66 | Host | enters the system | 0 | 1 |
| 931.7834 | Queue | 66 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 932.6365 | Move | 65 | Host | service done, routing to Waiters | 1 | 0 |
| 932.6365 | Seize | 66 | Host | pulled from queue after waiting 0.8531, service until 934.6146 | 0 | 1 |
| 932.6365 | Seize | 65 | Waiters | server free, service 43.1235 until 975.7600 | 0 | 3 |
| 934.6146 | Move | 66 | Host | service done, routing to Waiters | 0 | 0 |
| 934.6146 | Queue | 66 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 944.8960 | Arrival | 67 | Host | enters the system | 0 | 0 |
| 944.8960 | Seize | 67 | Host | server free, service 2.5892 until 947.4852 | 0 | 1 |
| 947.4852 | Move | 67 | Host | service done, routing to Waiters | 0 | 0 |
| 947.4852 | Queue | 67 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 947.9712 | Move | 63 | Waiters | service done, routing to Cashier | 2 | 2 |
| 947.9712 | Seize | 66 | Waiters | pulled from queue after waiting 13.3566, service until 1002.5359 | 1 | 3 |
| 947.9712 | Seize | 63 | Cashier | server free, service 2.1333 until 950.1045 | 0 | 1 |
| 949.0768 | Arrival | 68 | Host | enters the system | 0 | 0 |
| 949.0768 | Seize | 68 | Host | server free, service 0.8766 until 949.9534 | 0 | 1 |
| 949.9534 | Move | 68 | Host | service done, routing to Waiters | 0 | 0 |
| 949.9534 | Queue | 68 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 950.1045 | Exit | 63 | Cashier | exits; total wait 0.0000, time in system 41.5430 | 0 | 0 |
| 951.8660 | Arrival | 69 | Host | enters the system | 0 | 0 |
| 951.8660 | Seize | 69 | Host | server free, service 1.6555 until 953.5215 | 0 | 1 |
| 953.5215 | Move | 69 | Host | service done, routing to Waiters | 0 | 0 |
| 953.5215 | Queue | 69 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 957.1133 | Move | 64 | Waiters | service done, routing to Cashier | 3 | 2 |
| 957.1133 | Seize | 67 | Waiters | pulled from queue after waiting 9.6282, service until 997.7260 | 2 | 3 |
| 957.1133 | Seize | 64 | Cashier | server free, service 3.0751 until 960.1884 | 0 | 1 |
| 960.1884 | Exit | 64 | Cashier | exits; total wait 0.0000, time in system 48.2139 | 0 | 0 |
| 965.7938 | Arrival | 70 | Host | enters the system | 0 | 0 |
| 965.7938 | Seize | 70 | Host | server free, service 0.3967 until 966.1905 | 0 | 1 |
| 966.1905 | Move | 70 | Host | service done, routing to Waiters | 0 | 0 |
| 966.1905 | Queue | 70 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 974.2725 | Arrival | 71 | Host | enters the system | 0 | 0 |
| 974.2725 | Seize | 71 | Host | server free, service 1.0997 until 975.3722 | 0 | 1 |
| 975.3722 | Move | 71 | Host | service done, routing to Waiters | 0 | 0 |
| 975.3722 | Queue | 71 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 975.7600 | Move | 65 | Waiters | service done, routing to Cashier | 4 | 2 |
| 975.7600 | Seize | 68 | Waiters | pulled from queue after waiting 25.8066, service until 1016.5905 | 3 | 3 |
| 975.7600 | Seize | 65 | Cashier | server free, service 3.7124 until 979.4724 | 0 | 1 |
| 979.4724 | Exit | 65 | Cashier | exits; total wait 0.0000, time in system 48.6007 | 0 | 0 |
| 988.6703 | Arrival | 72 | Host | enters the system | 0 | 0 |
| 988.6703 | Seize | 72 | Host | server free, service 0.5011 until 989.1714 | 0 | 1 |
| 989.1714 | Move | 72 | Host | service done, routing to Waiters | 0 | 0 |
| 989.1714 | Queue | 72 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 997.7260 | Move | 67 | Waiters | service done, routing to Cashier | 4 | 2 |
| 997.7260 | Seize | 69 | Waiters | pulled from queue after waiting 44.2045, service until 1028.7513 | 3 | 3 |
| 997.7260 | Seize | 67 | Cashier | server free, service 3.7854 until 1001.5113 | 0 | 1 |
| 1001.5113 | Exit | 67 | Cashier | exits; total wait 9.6282, time in system 56.6153 | 0 | 0 |
| 1002.3835 | Arrival | 73 | Host | enters the system | 0 | 0 |
| 1002.3835 | Seize | 73 | Host | server free, service 1.0677 until 1003.4511 | 0 | 1 |
| 1002.5359 | Move | 66 | Waiters | service done, routing to Cashier | 3 | 2 |
| 1002.5359 | Seize | 70 | Waiters | pulled from queue after waiting 36.3454, service until 1054.2366 | 2 | 3 |
| 1002.5359 | Seize | 66 | Cashier | server free, service 1.8837 until 1004.4197 | 0 | 1 |
| 1003.4511 | Move | 73 | Host | service done, routing to Waiters | 0 | 0 |
| 1003.4511 | Queue | 73 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 1004.4197 | Exit | 66 | Cashier | exits; total wait 14.2098, time in system 72.6363 | 0 | 0 |
| 1016.5905 | Move | 68 | Waiters | service done, routing to Cashier | 3 | 2 |
| 1016.5905 | Seize | 71 | Waiters | pulled from queue after waiting 41.2183, service until 1058.5297 | 2 | 3 |
| 1016.5905 | Seize | 68 | Cashier | server free, service 3.1518 until 1019.7423 | 0 | 1 |
| 1019.7423 | Exit | 68 | Cashier | exits; total wait 25.8066, time in system 70.6655 | 0 | 0 |
| 1028.7513 | Move | 69 | Waiters | service done, routing to Cashier | 2 | 2 |
| 1028.7513 | Seize | 72 | Waiters | pulled from queue after waiting 39.5799, service until 1071.1124 | 1 | 3 |
| 1028.7513 | Seize | 69 | Cashier | server free, service 1.7945 until 1030.5458 | 0 | 1 |
| 1029.5558 | Arrival | 74 | Host | enters the system | 0 | 0 |
| 1029.5558 | Seize | 74 | Host | server free, service 0.6356 until 1030.1914 | 0 | 1 |
| 1030.1914 | Move | 74 | Host | service done, routing to Waiters | 0 | 0 |
| 1030.1914 | Queue | 74 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 1030.5458 | Exit | 69 | Cashier | exits; total wait 44.2045, time in system 78.6799 | 0 | 0 |
| 1054.2366 | Move | 70 | Waiters | service done, routing to Cashier | 2 | 2 |
| 1054.2366 | Seize | 73 | Waiters | pulled from queue after waiting 50.7855, service until 1087.1093 | 1 | 3 |
| 1054.2366 | Seize | 70 | Cashier | server free, service 3.5280 until 1057.7646 | 0 | 1 |
| 1057.7646 | Exit | 70 | Cashier | exits; total wait 36.3454, time in system 91.9708 | 0 | 0 |
| 1058.5297 | Move | 71 | Waiters | service done, routing to Cashier | 1 | 2 |
| 1058.5297 | Seize | 74 | Waiters | pulled from queue after waiting 28.3383, service until 1115.4742 | 0 | 3 |
| 1058.5297 | Seize | 71 | Cashier | server free, service 3.0760 until 1061.6057 | 0 | 1 |
| 1061.5881 | Arrival | 75 | Host | enters the system | 0 | 0 |
| 1061.5881 | Seize | 75 | Host | server free, service 0.1407 until 1061.7287 | 0 | 1 |
| 1061.6057 | Exit | 71 | Cashier | exits; total wait 41.2183, time in system 87.3332 | 0 | 0 |
| 1061.7287 | Move | 75 | Host | service done, routing to Waiters | 0 | 0 |
| 1061.7287 | Queue | 75 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 1064.8754 | Arrival | 76 | Host | enters the system | 0 | 0 |
| 1064.8754 | Seize | 76 | Host | server free, service 6.5792 until 1071.4547 | 0 | 1 |
| 1071.1124 | Move | 72 | Waiters | service done, routing to Cashier | 1 | 2 |
| 1071.1124 | Seize | 75 | Waiters | pulled from queue after waiting 9.3837, service until 1125.0783 | 0 | 3 |
| 1071.1124 | Seize | 72 | Cashier | server free, service 2.1737 until 1073.2861 | 0 | 1 |
| 1071.4547 | Move | 76 | Host | service done, routing to Waiters | 0 | 0 |
| 1071.4547 | Queue | 76 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 1073.2861 | Exit | 72 | Cashier | exits; total wait 39.5799, time in system 84.6158 | 0 | 0 |
| 1080.0947 | Arrival | 77 | Host | enters the system | 0 | 0 |
| 1080.0947 | Seize | 77 | Host | server free, service 0.5387 until 1080.6334 | 0 | 1 |
| 1080.6334 | Move | 77 | Host | service done, routing to Waiters | 0 | 0 |
| 1080.6334 | Queue | 77 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 1087.1093 | Move | 73 | Waiters | service done, routing to Cashier | 2 | 2 |
| 1087.1093 | Seize | 76 | Waiters | pulled from queue after waiting 15.6546, service until 1129.7457 | 1 | 3 |
| 1087.1093 | Seize | 73 | Cashier | server free, service 1.7095 until 1088.8187 | 0 | 1 |
| 1088.8187 | Exit | 73 | Cashier | exits; total wait 50.7855, time in system 86.4353 | 0 | 0 |
| 1115.4742 | Move | 74 | Waiters | service done, routing to Cashier | 1 | 2 |
| 1115.4742 | Seize | 77 | Waiters | pulled from queue after waiting 34.8409, service until 1138.1896 | 0 | 3 |
| 1115.4742 | Seize | 74 | Cashier | server free, service 1.1979 until 1116.6721 | 0 | 1 |
| 1116.0362 | Arrival | 78 | Host | enters the system | 0 | 0 |
| 1116.0362 | Seize | 78 | Host | server free, service 1.9813 until 1118.0175 | 0 | 1 |
| 1116.6721 | Exit | 74 | Cashier | exits; total wait 28.3383, time in system 87.1163 | 0 | 0 |
| 1118.0175 | Move | 78 | Host | service done, routing to Waiters | 0 | 0 |
| 1118.0175 | Queue | 78 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 1125.0783 | Move | 75 | Waiters | service done, routing to Cashier | 1 | 2 |
| 1125.0783 | Seize | 78 | Waiters | pulled from queue after waiting 7.0607, service until 1179.6154 | 0 | 3 |
| 1125.0783 | Seize | 75 | Cashier | server free, service 2.5491 until 1127.6273 | 0 | 1 |
| 1127.6273 | Exit | 75 | Cashier | exits; total wait 9.3837, time in system 66.0393 | 0 | 0 |
| 1129.7457 | Move | 76 | Waiters | service done, routing to Cashier | 0 | 2 |
| 1129.7457 | Seize | 76 | Cashier | server free, service 2.3037 until 1132.0494 | 0 | 1 |
| 1132.0494 | Exit | 76 | Cashier | exits; total wait 15.6546, time in system 67.1740 | 0 | 0 |
| 1138.1896 | Move | 77 | Waiters | service done, routing to Cashier | 0 | 1 |
| 1138.1896 | Seize | 77 | Cashier | server free, service 3.6111 until 1141.8007 | 0 | 1 |
| 1141.8007 | Exit | 77 | Cashier | exits; total wait 34.8409, time in system 61.7060 | 0 | 0 |
| 1144.8309 | Arrival | 79 | Host | enters the system | 0 | 0 |
| 1144.8309 | Seize | 79 | Host | server free, service 0.2315 until 1145.0623 | 0 | 1 |
| 1145.0623 | Move | 79 | Host | service done, routing to Waiters | 0 | 0 |
| 1145.0623 | Seize | 79 | Waiters | server free, service 42.5652 until 1187.6276 | 0 | 2 |
| 1165.6439 | Arrival | 80 | Host | enters the system | 0 | 0 |
| 1165.6439 | Seize | 80 | Host | server free, service 0.2921 until 1165.9359 | 0 | 1 |
| 1165.9359 | Move | 80 | Host | service done, routing to Waiters | 0 | 0 |
| 1165.9359 | Seize | 80 | Waiters | server free, service 37.0161 until 1202.9520 | 0 | 3 |
| 1179.6154 | Move | 78 | Waiters | service done, routing to Cashier | 0 | 2 |
| 1179.6154 | Seize | 78 | Cashier | server free, service 2.0192 until 1181.6346 | 0 | 1 |
| 1181.6346 | Exit | 78 | Cashier | exits; total wait 7.0607, time in system 65.5984 | 0 | 0 |
| 1187.6276 | Move | 79 | Waiters | service done, routing to Cashier | 0 | 1 |
| 1187.6276 | Seize | 79 | Cashier | server free, service 2.3935 until 1190.0211 | 0 | 1 |
| 1190.0211 | Exit | 79 | Cashier | exits; total wait 0.0000, time in system 45.1902 | 0 | 0 |
| 1198.9173 | Arrival | 81 | Host | enters the system | 0 | 0 |
| 1198.9173 | Seize | 81 | Host | server free, service 1.0258 until 1199.9431 | 0 | 1 |
| 1199.9431 | Move | 81 | Host | service done, routing to Waiters | 0 | 0 |
| 1199.9431 | Seize | 81 | Waiters | server free, service 32.4797 until 1232.4228 | 0 | 2 |
| 1201.6984 | Arrival | 82 | Host | enters the system | 0 | 0 |
| 1201.6984 | Seize | 82 | Host | server free, service 1.0139 until 1202.7124 | 0 | 1 |
| 1202.5490 | Arrival | 83 | Host | enters the system | 0 | 1 |
| 1202.5490 | Queue | 83 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 1202.7124 | Move | 82 | Host | service done, routing to Waiters | 1 | 0 |
| 1202.7124 | Seize | 83 | Host | pulled from queue after waiting 0.1633, service until 1203.8087 | 0 | 1 |
| 1202.7124 | Seize | 82 | Waiters | server free, service 34.9255 until 1237.6378 | 0 | 3 |
| 1202.9520 | Move | 80 | Waiters | service done, routing to Cashier | 0 | 2 |
| 1202.9520 | Seize | 80 | Cashier | server free, service 2.5914 until 1205.5433 | 0 | 1 |
| 1203.8087 | Move | 83 | Host | service done, routing to Waiters | 0 | 0 |
| 1203.8087 | Seize | 83 | Waiters | server free, service 42.2304 until 1246.0391 | 0 | 3 |
| 1205.5433 | Exit | 80 | Cashier | exits; total wait 0.0000, time in system 39.8995 | 0 | 0 |
| 1219.7527 | Arrival | 84 | Host | enters the system | 0 | 0 |
| 1219.7527 | Seize | 84 | Host | server free, service 2.0724 until 1221.8251 | 0 | 1 |
| 1221.5568 | Arrival | 85 | Host | enters the system | 0 | 1 |
| 1221.5568 | Queue | 85 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 1221.8251 | Move | 84 | Host | service done, routing to Waiters | 1 | 0 |
| 1221.8251 | Seize | 85 | Host | pulled from queue after waiting 0.2683, service until 1226.5902 | 0 | 1 |
| 1221.8251 | Queue | 84 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 1224.6383 | Arrival | 86 | Host | enters the system | 0 | 1 |
| 1224.6383 | Queue | 86 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 1226.5902 | Move | 85 | Host | service done, routing to Waiters | 1 | 0 |
| 1226.5902 | Seize | 86 | Host | pulled from queue after waiting 1.9519, service until 1227.6749 | 0 | 1 |
| 1226.5902 | Queue | 85 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 1227.6749 | Move | 86 | Host | service done, routing to Waiters | 0 | 0 |
| 1227.6749 | Queue | 86 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 1232.4228 | Move | 81 | Waiters | service done, routing to Cashier | 3 | 2 |
| 1232.4228 | Seize | 84 | Waiters | pulled from queue after waiting 10.5977, service until 1271.4781 | 2 | 3 |
| 1232.4228 | Seize | 81 | Cashier | server free, service 2.9398 until 1235.3627 | 0 | 1 |
| 1235.3627 | Exit | 81 | Cashier | exits; total wait 0.0000, time in system 36.4454 | 0 | 0 |
| 1237.6378 | Move | 82 | Waiters | service done, routing to Cashier | 2 | 2 |
| 1237.6378 | Seize | 85 | Waiters | pulled from queue after waiting 11.0476, service until 1282.6557 | 1 | 3 |
| 1237.6378 | Seize | 82 | Cashier | server free, service 2.4322 until 1240.0700 | 0 | 1 |
| 1240.0700 | Exit | 82 | Cashier | exits; total wait 0.0000, time in system 38.3715 | 0 | 0 |
| 1245.3411 | Arrival | 87 | Host | enters the system | 0 | 0 |
| 1245.3411 | Seize | 87 | Host | server free, service 1.6341 until 1246.9752 | 0 | 1 |
| 1246.0391 | Move | 83 | Waiters | service done, routing to Cashier | 1 | 2 |
| 1246.0391 | Seize | 86 | Waiters | pulled from queue after waiting 18.3642, service until 1271.0094 | 0 | 3 |
| 1246.0391 | Seize | 83 | Cashier | server free, service 3.2310 until 1249.2701 | 0 | 1 |
| 1246.9752 | Move | 87 | Host | service done, routing to Waiters | 0 | 0 |
| 1246.9752 | Queue | 87 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 1249.2701 | Exit | 83 | Cashier | exits; total wait 0.1633, time in system 46.7211 | 0 | 0 |
| 1254.2744 | Arrival | 88 | Host | enters the system | 0 | 0 |
| 1254.2744 | Seize | 88 | Host | server free, service 0.5873 until 1254.8617 | 0 | 1 |
| 1254.8617 | Move | 88 | Host | service done, routing to Waiters | 0 | 0 |
| 1254.8617 | Queue | 88 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 1270.9079 | Arrival | 89 | Host | enters the system | 0 | 0 |
| 1270.9079 | Seize | 89 | Host | server free, service 2.7665 until 1273.6744 | 0 | 1 |
| 1271.0094 | Move | 86 | Waiters | service done, routing to Cashier | 2 | 2 |
| 1271.0094 | Seize | 87 | Waiters | pulled from queue after waiting 24.0342, service until 1318.4641 | 1 | 3 |
| 1271.0094 | Seize | 86 | Cashier | server free, service 3.6176 until 1274.6270 | 0 | 1 |
| 1271.4781 | Move | 84 | Waiters | service done, routing to Cashier | 1 | 2 |
| 1271.4781 | Seize | 88 | Waiters | pulled from queue after waiting 16.6164, service until 1309.3547 | 0 | 3 |
| 1271.4781 | Queue | 84 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 1273.6744 | Move | 89 | Host | service done, routing to Waiters | 0 | 0 |
| 1273.6744 | Queue | 89 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 1274.6270 | Exit | 86 | Cashier | exits; total wait 20.3161, time in system 49.9887 | 1 | 0 |
| 1274.6270 | Seize | 84 | Cashier | pulled from queue after waiting 3.1489, service until 1277.0269 | 0 | 1 |
| 1277.0269 | Exit | 84 | Cashier | exits; total wait 13.7466, time in system 57.2742 | 0 | 0 |
| 1282.6557 | Move | 85 | Waiters | service done, routing to Cashier | 1 | 2 |
| 1282.6557 | Seize | 89 | Waiters | pulled from queue after waiting 8.9813, service until 1332.6058 | 0 | 3 |
| 1282.6557 | Seize | 85 | Cashier | server free, service 1.8853 until 1284.5410 | 0 | 1 |
| 1284.5410 | Exit | 85 | Cashier | exits; total wait 11.3159, time in system 62.9841 | 0 | 0 |
| 1306.9760 | Arrival | 90 | Host | enters the system | 0 | 0 |
| 1306.9760 | Seize | 90 | Host | server free, service 1.1656 until 1308.1415 | 0 | 1 |
| 1308.1415 | Move | 90 | Host | service done, routing to Waiters | 0 | 0 |
| 1308.1415 | Queue | 90 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 1309.3547 | Move | 88 | Waiters | service done, routing to Cashier | 1 | 2 |
| 1309.3547 | Seize | 90 | Waiters | pulled from queue after waiting 1.2131, service until 1335.4222 | 0 | 3 |
| 1309.3547 | Seize | 88 | Cashier | server free, service 3.8630 until 1313.2176 | 0 | 1 |
| 1313.2176 | Exit | 88 | Cashier | exits; total wait 16.6164, time in system 58.9433 | 0 | 0 |
| 1318.4641 | Move | 87 | Waiters | service done, routing to Cashier | 0 | 2 |
| 1318.4641 | Seize | 87 | Cashier | server free, service 2.2665 until 1320.7306 | 0 | 1 |
| 1320.7306 | Exit | 87 | Cashier | exits; total wait 24.0342, time in system 75.3895 | 0 | 0 |
| 1321.1238 | Arrival | 91 | Host | enters the system | 0 | 0 |
| 1321.1238 | Seize | 91 | Host | server free, service 0.8186 until 1321.9424 | 0 | 1 |
| 1321.9424 | Move | 91 | Host | service done, routing to Waiters | 0 | 0 |
| 1321.9424 | Seize | 91 | Waiters | server free, service 45.8787 until 1367.8211 | 0 | 3 |
| 1332.6058 | Move | 89 | Waiters | service done, routing to Cashier | 0 | 2 |
| 1332.6058 | Seize | 89 | Cashier | server free, service 1.0079 until 1333.6137 | 0 | 1 |
| 1333.6137 | Exit | 89 | Cashier | exits; total wait 8.9813, time in system 62.7058 | 0 | 0 |
| 1333.7285 | Arrival | 92 | Host | enters the system | 0 | 0 |
| 1333.7285 | Seize | 92 | Host | server free, service 1.1873 until 1334.9159 | 0 | 1 |
| 1334.9159 | Move | 92 | Host | service done, routing to Waiters | 0 | 0 |
| 1334.9159 | Seize | 92 | Waiters | server free, service 37.4904 until 1372.4062 | 0 | 3 |
| 1335.4222 | Move | 90 | Waiters | service done, routing to Cashier | 0 | 2 |
| 1335.4222 | Seize | 90 | Cashier | server free, service 1.8420 until 1337.2642 | 0 | 1 |
| 1337.2642 | Exit | 90 | Cashier | exits; total wait 1.2131, time in system 30.2882 | 0 | 0 |
| 1362.9965 | Arrival | 93 | Host | enters the system | 0 | 0 |
| 1362.9965 | Seize | 93 | Host | server free, service 1.0763 until 1364.0728 | 0 | 1 |
| 1364.0728 | Move | 93 | Host | service done, routing to Waiters | 0 | 0 |
| 1364.0728 | Seize | 93 | Waiters | server free, service 54.4025 until 1418.4753 | 0 | 3 |
| 1367.8211 | Move | 91 | Waiters | service done, routing to Cashier | 0 | 2 |
| 1367.8211 | Seize | 91 | Cashier | server free, service 2.1787 until 1369.9998 | 0 | 1 |
| 1369.9998 | Exit | 91 | Cashier | exits; total wait 0.0000, time in system 48.8759 | 0 | 0 |
| 1372.4062 | Move | 92 | Waiters | service done, routing to Cashier | 0 | 1 |
| 1372.4062 | Seize | 92 | Cashier | server free, service 3.5269 until 1375.9331 | 0 | 1 |
| 1375.9331 | Exit | 92 | Cashier | exits; total wait 0.0000, time in system 42.2046 | 0 | 0 |
| 1394.5110 | Arrival | 94 | Host | enters the system | 0 | 0 |
| 1394.5110 | Seize | 94 | Host | server free, service 0.1510 until 1394.6620 | 0 | 1 |
| 1394.6620 | Move | 94 | Host | service done, routing to Waiters | 0 | 0 |
| 1394.6620 | Seize | 94 | Waiters | server free, service 31.2417 until 1425.9036 | 0 | 2 |
| 1413.8769 | Arrival | 95 | Host | enters the system | 0 | 0 |
| 1413.8769 | Seize | 95 | Host | server free, service 11.0945 until 1424.9715 | 0 | 1 |
| 1418.4753 | Move | 93 | Waiters | service done, routing to Cashier | 0 | 1 |
| 1418.4753 | Seize | 93 | Cashier | server free, service 1.2466 until 1419.7219 | 0 | 1 |
| 1419.7219 | Exit | 93 | Cashier | exits; total wait 0.0000, time in system 56.7255 | 0 | 0 |
| 1424.9715 | Move | 95 | Host | service done, routing to Waiters | 0 | 0 |
| 1424.9715 | Seize | 95 | Waiters | server free, service 35.4720 until 1460.4435 | 0 | 2 |
| 1425.9036 | Move | 94 | Waiters | service done, routing to Cashier | 0 | 1 |
| 1425.9036 | Seize | 94 | Cashier | server free, service 1.0152 until 1426.9189 | 0 | 1 |
| 1426.9189 | Exit | 94 | Cashier | exits; total wait 0.0000, time in system 32.4079 | 0 | 0 |
| 1429.2047 | Arrival | 96 | Host | enters the system | 0 | 0 |
| 1429.2047 | Seize | 96 | Host | server free, service 0.6177 until 1429.8223 | 0 | 1 |
| 1429.8223 | Move | 96 | Host | service done, routing to Waiters | 0 | 0 |
| 1429.8223 | Seize | 96 | Waiters | server free, service 58.9314 until 1488.7537 | 0 | 2 |
| 1438.9383 | Arrival | 97 | Host | enters the system | 0 | 0 |
| 1438.9383 | Seize | 97 | Host | server free, service 1.7451 until 1440.6834 | 0 | 1 |
| 1439.9215 | Arrival | 98 | Host | enters the system | 0 | 1 |
| 1439.9215 | Queue | 98 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 1440.6834 | Move | 97 | Host | service done, routing to Waiters | 1 | 0 |
| 1440.6834 | Seize | 98 | Host | pulled from queue after waiting 0.7618, service until 1446.3262 | 0 | 1 |
| 1440.6834 | Seize | 97 | Waiters | server free, service 27.6534 until 1468.3367 | 0 | 3 |
| 1446.3262 | Move | 98 | Host | service done, routing to Waiters | 0 | 0 |
| 1446.3262 | Queue | 98 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 1460.4435 | Move | 95 | Waiters | service done, routing to Cashier | 1 | 2 |
| 1460.4435 | Seize | 98 | Waiters | pulled from queue after waiting 14.1173, service until 1516.9185 | 0 | 3 |
| 1460.4435 | Seize | 95 | Cashier | server free, service 3.9631 until 1464.4066 | 0 | 1 |
| 1464.4066 | Exit | 95 | Cashier | exits; total wait 0.0000, time in system 50.5297 | 0 | 0 |
| 1468.3367 | Move | 97 | Waiters | service done, routing to Cashier | 0 | 2 |
| 1468.3367 | Seize | 97 | Cashier | server free, service 2.4460 until 1470.7827 | 0 | 1 |
| 1470.7827 | Exit | 97 | Cashier | exits; total wait 0.0000, time in system 31.8444 | 0 | 0 |
| 1488.7537 | Move | 96 | Waiters | service done, routing to Cashier | 0 | 1 |
| 1488.7537 | Seize | 96 | Cashier | server free, service 3.3417 until 1492.0955 | 0 | 1 |
| 1488.9489 | Arrival | 99 | Host | enters the system | 0 | 0 |
| 1488.9489 | Seize | 99 | Host | server free, service 3.0462 until 1491.9951 | 0 | 1 |
| 1491.9951 | Move | 99 | Host | service done, routing to Waiters | 0 | 0 |
| 1491.9951 | Seize | 99 | Waiters | server free, service 34.1510 until 1526.1461 | 0 | 2 |
| 1492.0955 | Exit | 96 | Cashier | exits; total wait 0.0000, time in system 62.8908 | 0 | 0 |
| 1508.5783 | Arrival | 100 | Host | enters the system | 0 | 0 |
| 1508.5783 | Seize | 100 | Host | server free, service 0.8135 until 1509.3918 | 0 | 1 |
| 1509.3918 | Move | 100 | Host | service done, routing to Waiters | 0 | 0 |
| 1509.3918 | Seize | 100 | Waiters | server free, service 44.7652 until 1554.1570 | 0 | 3 |
| 1516.9185 | Move | 98 | Waiters | service done, routing to Cashier | 0 | 2 |
| 1516.9185 | Seize | 98 | Cashier | server free, service 1.0049 until 1517.9234 | 0 | 1 |
| 1517.9234 | Exit | 98 | Cashier | exits; total wait 14.8791, time in system 78.0018 | 0 | 0 |
| 1520.2313 | Arrival | 101 | Host | enters the system | 0 | 0 |
| 1520.2313 | Seize | 101 | Host | server free, service 0.0461 until 1520.2774 | 0 | 1 |
| 1520.2774 | Move | 101 | Host | service done, routing to Waiters | 0 | 0 |
| 1520.2774 | Seize | 101 | Waiters | server free, service 33.3306 until 1553.6080 | 0 | 3 |
| 1526.1461 | Move | 99 | Waiters | service done, routing to Cashier | 0 | 2 |
| 1526.1461 | Seize | 99 | Cashier | server free, service 1.0875 until 1527.2336 | 0 | 1 |
| 1527.2336 | Exit | 99 | Cashier | exits; total wait 0.0000, time in system 38.2847 | 0 | 0 |
| 1531.4310 | Arrival | 102 | Host | enters the system | 0 | 0 |
| 1531.4310 | Seize | 102 | Host | server free, service 0.1813 until 1531.6123 | 0 | 1 |
| 1531.6123 | Move | 102 | Host | service done, routing to Waiters | 0 | 0 |
| 1531.6123 | Seize | 102 | Waiters | server free, service 40.5230 until 1572.1354 | 0 | 3 |
| 1553.6080 | Move | 101 | Waiters | service done, routing to Cashier | 0 | 2 |
| 1553.6080 | Seize | 101 | Cashier | server free, service 1.2188 until 1554.8268 | 0 | 1 |
| 1554.1570 | Move | 100 | Waiters | service done, routing to Cashier | 0 | 1 |
| 1554.1570 | Queue | 100 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 1554.8268 | Exit | 101 | Cashier | exits; total wait 0.0000, time in system 34.5955 | 1 | 0 |
| 1554.8268 | Seize | 100 | Cashier | pulled from queue after waiting 0.6698, service until 1557.0298 | 0 | 1 |
| 1557.0298 | Exit | 100 | Cashier | exits; total wait 0.6698, time in system 48.4515 | 0 | 0 |
| 1572.1354 | Move | 102 | Waiters | service done, routing to Cashier | 0 | 0 |
| 1572.1354 | Seize | 102 | Cashier | server free, service 2.5119 until 1574.6473 | 0 | 1 |
| 1574.6473 | Exit | 102 | Cashier | exits; total wait 0.0000, time in system 43.2163 | 0 | 0 |
| 1595.3308 | Arrival | 103 | Host | enters the system | 0 | 0 |
| 1595.3308 | Seize | 103 | Host | server free, service 1.6283 until 1596.9591 | 0 | 1 |
| 1595.8566 | Arrival | 104 | Host | enters the system | 0 | 1 |
| 1595.8566 | Queue | 104 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 1596.9591 | Move | 103 | Host | service done, routing to Waiters | 1 | 0 |
| 1596.9591 | Seize | 104 | Host | pulled from queue after waiting 1.1025, service until 1599.1155 | 0 | 1 |
| 1596.9591 | Seize | 103 | Waiters | server free, service 49.6329 until 1646.5920 | 0 | 1 |
| 1599.1155 | Move | 104 | Host | service done, routing to Waiters | 0 | 0 |
| 1599.1155 | Seize | 104 | Waiters | server free, service 46.0554 until 1645.1709 | 0 | 2 |
| 1642.7870 | Arrival | 105 | Host | enters the system | 0 | 0 |
| 1642.7870 | Seize | 105 | Host | server free, service 4.5653 until 1647.3523 | 0 | 1 |
| 1645.1709 | Move | 104 | Waiters | service done, routing to Cashier | 0 | 1 |
| 1645.1709 | Seize | 104 | Cashier | server free, service 3.6385 until 1648.8094 | 0 | 1 |
| 1646.5920 | Move | 103 | Waiters | service done, routing to Cashier | 0 | 0 |
| 1646.5920 | Queue | 103 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 1647.3523 | Move | 105 | Host | service done, routing to Waiters | 0 | 0 |
| 1647.3523 | Seize | 105 | Waiters | server free, service 58.6538 until 1706.0061 | 0 | 1 |
| 1648.8094 | Exit | 104 | Cashier | exits; total wait 1.1025, time in system 52.9528 | 1 | 0 |
| 1648.8094 | Seize | 103 | Cashier | pulled from queue after waiting 2.2174, service until 1650.3174 | 0 | 1 |
| 1650.3174 | Exit | 103 | Cashier | exits; total wait 2.2174, time in system 54.9866 | 0 | 0 |
| 1662.9524 | Arrival | 106 | Host | enters the system | 0 | 0 |
| 1662.9524 | Seize | 106 | Host | server free, service 1.0116 until 1663.9640 | 0 | 1 |
| 1663.9640 | Move | 106 | Host | service done, routing to Waiters | 0 | 0 |
| 1663.9640 | Seize | 106 | Waiters | server free, service 47.1694 until 1711.1334 | 0 | 2 |
| 1669.0084 | Arrival | 107 | Host | enters the system | 0 | 0 |
| 1669.0084 | Seize | 107 | Host | server free, service 5.3627 until 1674.3711 | 0 | 1 |
| 1674.3711 | Move | 107 | Host | service done, routing to Waiters | 0 | 0 |
| 1674.3711 | Seize | 107 | Waiters | server free, service 31.6232 until 1705.9943 | 0 | 3 |
| 1705.9943 | Move | 107 | Waiters | service done, routing to Cashier | 0 | 2 |
| 1705.9943 | Seize | 107 | Cashier | server free, service 1.3525 until 1707.3468 | 0 | 1 |
| 1706.0061 | Move | 105 | Waiters | service done, routing to Cashier | 0 | 1 |
| 1706.0061 | Queue | 105 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 1707.3468 | Exit | 107 | Cashier | exits; total wait 0.0000, time in system 38.3384 | 1 | 0 |
| 1707.3468 | Seize | 105 | Cashier | pulled from queue after waiting 1.3407, service until 1710.4537 | 0 | 1 |
| 1710.4537 | Exit | 105 | Cashier | exits; total wait 1.3407, time in system 67.6668 | 0 | 0 |
| 1711.1334 | Move | 106 | Waiters | service done, routing to Cashier | 0 | 0 |
| 1711.1334 | Seize | 106 | Cashier | server free, service 2.0882 until 1713.2216 | 0 | 1 |
| 1713.2216 | Exit | 106 | Cashier | exits; total wait 0.0000, time in system 50.2692 | 0 | 0 |
| 1734.4476 | Arrival | 108 | Host | enters the system | 0 | 0 |
| 1734.4476 | Seize | 108 | Host | server free, service 1.2387 until 1735.6863 | 0 | 1 |
| 1735.6863 | Move | 108 | Host | service done, routing to Waiters | 0 | 0 |
| 1735.6863 | Seize | 108 | Waiters | server free, service 36.5155 until 1772.2019 | 0 | 1 |
| 1766.2239 | Arrival | 109 | Host | enters the system | 0 | 0 |
| 1766.2239 | Seize | 109 | Host | server free, service 2.7924 until 1769.0163 | 0 | 1 |
| 1769.0163 | Move | 109 | Host | service done, routing to Waiters | 0 | 0 |
| 1769.0163 | Seize | 109 | Waiters | server free, service 31.4904 until 1800.5067 | 0 | 2 |
| 1769.9675 | Arrival | 110 | Host | enters the system | 0 | 0 |
| 1769.9675 | Seize | 110 | Host | server free, service 2.9596 until 1772.9271 | 0 | 1 |
| 1772.2019 | Move | 108 | Waiters | service done, routing to Cashier | 0 | 1 |
| 1772.2019 | Seize | 108 | Cashier | server free, service 1.7049 until 1773.9067 | 0 | 1 |
| 1772.9271 | Move | 110 | Host | service done, routing to Waiters | 0 | 0 |
| 1772.9271 | Seize | 110 | Waiters | server free, service 33.9507 until 1806.8779 | 0 | 2 |
| 1773.9067 | Exit | 108 | Cashier | exits; total wait 0.0000, time in system 39.4592 | 0 | 0 |
| 1776.8590 | Arrival | 111 | Host | enters the system | 0 | 0 |
| 1776.8590 | Seize | 111 | Host | server free, service 3.5207 until 1780.3797 | 0 | 1 |
| 1777.9542 | Arrival | 112 | Host | enters the system | 0 | 1 |
| 1777.9542 | Queue | 112 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 1780.3797 | Move | 111 | Host | service done, routing to Waiters | 1 | 0 |
| 1780.3797 | Seize | 112 | Host | pulled from queue after waiting 2.4255, service until 1782.3079 | 0 | 1 |
| 1780.3797 | Seize | 111 | Waiters | server free, service 32.3131 until 1812.6928 | 0 | 3 |
| 1782.3079 | Move | 112 | Host | service done, routing to Waiters | 0 | 0 |
| 1782.3079 | Queue | 112 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 1788.3198 | Arrival | 113 | Host | enters the system | 0 | 0 |
| 1788.3198 | Seize | 113 | Host | server free, service 0.0758 until 1788.3956 | 0 | 1 |
| 1788.3956 | Move | 113 | Host | service done, routing to Waiters | 0 | 0 |
| 1788.3956 | Queue | 113 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 1800.5067 | Move | 109 | Waiters | service done, routing to Cashier | 2 | 2 |
| 1800.5067 | Seize | 112 | Waiters | pulled from queue after waiting 18.1988, service until 1832.6137 | 1 | 3 |
| 1800.5067 | Seize | 109 | Cashier | server free, service 2.4651 until 1802.9717 | 0 | 1 |
| 1802.9717 | Exit | 109 | Cashier | exits; total wait 0.0000, time in system 36.7479 | 0 | 0 |
| 1805.6314 | Arrival | 114 | Host | enters the system | 0 | 0 |
| 1805.6314 | Seize | 114 | Host | server free, service 1.5117 until 1807.1431 | 0 | 1 |
| 1806.8779 | Move | 110 | Waiters | service done, routing to Cashier | 1 | 2 |
| 1806.8779 | Seize | 113 | Waiters | pulled from queue after waiting 18.4822, service until 1841.1050 | 0 | 3 |
| 1806.8779 | Seize | 110 | Cashier | server free, service 3.8228 until 1810.7006 | 0 | 1 |
| 1807.1431 | Move | 114 | Host | service done, routing to Waiters | 0 | 0 |
| 1807.1431 | Queue | 114 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 1810.7006 | Exit | 110 | Cashier | exits; total wait 0.0000, time in system 40.7331 | 0 | 0 |
| 1812.6928 | Move | 111 | Waiters | service done, routing to Cashier | 1 | 2 |
| 1812.6928 | Seize | 114 | Waiters | pulled from queue after waiting 5.5497, service until 1848.5127 | 0 | 3 |
| 1812.6928 | Seize | 111 | Cashier | server free, service 3.6943 until 1816.3870 | 0 | 1 |
| 1816.3870 | Exit | 111 | Cashier | exits; total wait 0.0000, time in system 39.5281 | 0 | 0 |
| 1832.6137 | Move | 112 | Waiters | service done, routing to Cashier | 0 | 2 |
| 1832.6137 | Seize | 112 | Cashier | server free, service 1.1955 until 1833.8092 | 0 | 1 |
| 1833.8092 | Exit | 112 | Cashier | exits; total wait 20.6242, time in system 55.8550 | 0 | 0 |
| 1841.1050 | Move | 113 | Waiters | service done, routing to Cashier | 0 | 1 |
| 1841.1050 | Seize | 113 | Cashier | server free, service 3.7994 until 1844.9044 | 0 | 1 |
| 1844.9044 | Exit | 113 | Cashier | exits; total wait 18.4822, time in system 56.5846 | 0 | 0 |
| 1848.5127 | Move | 114 | Waiters | service done, routing to Cashier | 0 | 0 |
| 1848.5127 | Seize | 114 | Cashier | server free, service 3.0245 until 1851.5371 | 0 | 1 |
| 1850.4410 | Arrival | 115 | Host | enters the system | 0 | 0 |
| 1850.4410 | Seize | 115 | Host | server free, service 4.9100 until 1855.3510 | 0 | 1 |
| 1851.5371 | Exit | 114 | Cashier | exits; total wait 5.5497, time in system 45.9058 | 0 | 0 |
| 1855.0961 | Arrival | 116 | Host | enters the system | 0 | 1 |
| 1855.0961 | Queue | 116 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 1855.3510 | Move | 115 | Host | service done, routing to Waiters | 1 | 0 |
| 1855.3510 | Seize | 116 | Host | pulled from queue after waiting 0.2549, service until 1856.1046 | 0 | 1 |
| 1855.3510 | Seize | 115 | Waiters | server free, service 29.0362 until 1884.3872 | 0 | 1 |
| 1856.1046 | Move | 116 | Host | service done, routing to Waiters | 0 | 0 |
| 1856.1046 | Seize | 116 | Waiters | server free, service 47.9777 until 1904.0823 | 0 | 2 |
| 1857.8367 | Arrival | 117 | Host | enters the system | 0 | 0 |
| 1857.8367 | Seize | 117 | Host | server free, service 0.4129 until 1858.2496 | 0 | 1 |
| 1858.2496 | Move | 117 | Host | service done, routing to Waiters | 0 | 0 |
| 1858.2496 | Seize | 117 | Waiters | server free, service 44.0204 until 1902.2700 | 0 | 3 |
| 1884.3872 | Move | 115 | Waiters | service done, routing to Cashier | 0 | 2 |
| 1884.3872 | Seize | 115 | Cashier | server free, service 1.9759 until 1886.3632 | 0 | 1 |
| 1886.3632 | Exit | 115 | Cashier | exits; total wait 0.0000, time in system 35.9222 | 0 | 0 |
| 1888.1488 | Arrival | 118 | Host | enters the system | 0 | 0 |
| 1888.1488 | Seize | 118 | Host | server free, service 1.9448 until 1890.0936 | 0 | 1 |
| 1890.0936 | Move | 118 | Host | service done, routing to Waiters | 0 | 0 |
| 1890.0936 | Seize | 118 | Waiters | server free, service 40.3636 until 1930.4572 | 0 | 3 |
| 1893.4803 | Arrival | 119 | Host | enters the system | 0 | 0 |
| 1893.4803 | Seize | 119 | Host | server free, service 0.3207 until 1893.8011 | 0 | 1 |
| 1893.8011 | Move | 119 | Host | service done, routing to Waiters | 0 | 0 |
| 1893.8011 | Queue | 119 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 1900.9450 | Arrival | 120 | Host | enters the system | 0 | 0 |
| 1900.9450 | Seize | 120 | Host | server free, service 0.7576 until 1901.7026 | 0 | 1 |
| 1901.7026 | Move | 120 | Host | service done, routing to Waiters | 0 | 0 |
| 1901.7026 | Queue | 120 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 1902.2700 | Move | 117 | Waiters | service done, routing to Cashier | 2 | 2 |
| 1902.2700 | Seize | 119 | Waiters | pulled from queue after waiting 8.4689, service until 1941.6632 | 1 | 3 |
| 1902.2700 | Seize | 117 | Cashier | server free, service 3.1028 until 1905.3727 | 0 | 1 |
| 1904.0823 | Move | 116 | Waiters | service done, routing to Cashier | 1 | 2 |
| 1904.0823 | Seize | 120 | Waiters | pulled from queue after waiting 2.3798, service until 1956.3724 | 0 | 3 |
| 1904.0823 | Queue | 116 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 1905.3727 | Exit | 117 | Cashier | exits; total wait 0.0000, time in system 47.5361 | 1 | 0 |
| 1905.3727 | Seize | 116 | Cashier | pulled from queue after waiting 1.2904, service until 1908.5628 | 0 | 1 |
| 1908.1818 | Arrival | 121 | Host | enters the system | 0 | 0 |
| 1908.1818 | Seize | 121 | Host | server free, service 0.5723 until 1908.7542 | 0 | 1 |
| 1908.5628 | Exit | 116 | Cashier | exits; total wait 1.5453, time in system 53.4667 | 0 | 0 |
| 1908.7542 | Move | 121 | Host | service done, routing to Waiters | 0 | 0 |
| 1908.7542 | Queue | 121 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 1930.4572 | Move | 118 | Waiters | service done, routing to Cashier | 1 | 2 |
| 1930.4572 | Seize | 121 | Waiters | pulled from queue after waiting 21.7031, service until 1972.9296 | 0 | 3 |
| 1930.4572 | Seize | 118 | Cashier | server free, service 1.1819 until 1931.6392 | 0 | 1 |
| 1930.7591 | Arrival | 122 | Host | enters the system | 0 | 0 |
| 1930.7591 | Seize | 122 | Host | server free, service 5.5104 until 1936.2695 | 0 | 1 |
| 1931.6392 | Exit | 118 | Cashier | exits; total wait 0.0000, time in system 43.4904 | 0 | 0 |
| 1936.2695 | Move | 122 | Host | service done, routing to Waiters | 0 | 0 |
| 1936.2695 | Queue | 122 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 1941.6632 | Move | 119 | Waiters | service done, routing to Cashier | 1 | 2 |
| 1941.6632 | Seize | 122 | Waiters | pulled from queue after waiting 5.3937, service until 1970.0712 | 0 | 3 |
| 1941.6632 | Seize | 119 | Cashier | server free, service 3.3896 until 1945.0528 | 0 | 1 |
| 1941.9526 | Arrival | 123 | Host | enters the system | 0 | 0 |
| 1941.9526 | Seize | 123 | Host | server free, service 0.1235 until 1942.0762 | 0 | 1 |
| 1942.0762 | Move | 123 | Host | service done, routing to Waiters | 0 | 0 |
| 1942.0762 | Queue | 123 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 1944.6133 | Arrival | 124 | Host | enters the system | 0 | 0 |
| 1944.6133 | Seize | 124 | Host | server free, service 3.8014 until 1948.4147 | 0 | 1 |
| 1945.0528 | Exit | 119 | Cashier | exits; total wait 8.4689, time in system 51.5725 | 0 | 0 |
| 1948.4147 | Move | 124 | Host | service done, routing to Waiters | 0 | 0 |
| 1948.4147 | Queue | 124 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 1956.3724 | Move | 120 | Waiters | service done, routing to Cashier | 2 | 2 |
| 1956.3724 | Seize | 123 | Waiters | pulled from queue after waiting 14.2963, service until 1997.2047 | 1 | 3 |
| 1956.3724 | Seize | 120 | Cashier | server free, service 1.5507 until 1957.9232 | 0 | 1 |
| 1957.9232 | Exit | 120 | Cashier | exits; total wait 2.3798, time in system 56.9782 | 0 | 0 |
| 1969.7757 | Arrival | 125 | Host | enters the system | 0 | 0 |
| 1969.7757 | Seize | 125 | Host | server free, service 4.0476 until 1973.8233 | 0 | 1 |
| 1970.0712 | Move | 122 | Waiters | service done, routing to Cashier | 1 | 2 |
| 1970.0712 | Seize | 124 | Waiters | pulled from queue after waiting 21.6565, service until 2015.9839 | 0 | 3 |
| 1970.0712 | Seize | 122 | Cashier | server free, service 2.9550 until 1973.0261 | 0 | 1 |
| 1972.9296 | Move | 121 | Waiters | service done, routing to Cashier | 0 | 2 |
| 1972.9296 | Queue | 121 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 1973.0261 | Exit | 122 | Cashier | exits; total wait 5.3937, time in system 42.2670 | 1 | 0 |
| 1973.0261 | Seize | 121 | Cashier | pulled from queue after waiting 0.0965, service until 1976.9299 | 0 | 1 |
| 1973.8233 | Move | 125 | Host | service done, routing to Waiters | 0 | 0 |
| 1973.8233 | Seize | 125 | Waiters | server free, service 34.8113 until 2008.6346 | 0 | 3 |
| 1976.9299 | Exit | 121 | Cashier | exits; total wait 21.7996, time in system 68.7481 | 0 | 0 |
| 1992.9546 | Arrival | 126 | Host | enters the system | 0 | 0 |
| 1992.9546 | Seize | 126 | Host | server free, service 0.9810 until 1993.9356 | 0 | 1 |
| 1993.9356 | Move | 126 | Host | service done, routing to Waiters | 0 | 0 |
| 1993.9356 | Queue | 126 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 1994.6976 | Arrival | 127 | Host | enters the system | 0 | 0 |
| 1994.6976 | Seize | 127 | Host | server free, service 7.7765 until 2002.4740 | 0 | 1 |
| 1997.2047 | Move | 123 | Waiters | service done, routing to Cashier | 1 | 2 |
| 1997.2047 | Seize | 126 | Waiters | pulled from queue after waiting 3.2690, service until 2023.5917 | 0 | 3 |
| 1997.2047 | Seize | 123 | Cashier | server free, service 1.2225 until 1998.4271 | 0 | 1 |
| 1998.4271 | Exit | 123 | Cashier | exits; total wait 14.2963, time in system 56.4745 | 0 | 0 |
| 2002.4740 | Move | 127 | Host | service done, routing to Waiters | 0 | 0 |
| 2002.4740 | Queue | 127 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 2008.6346 | Move | 125 | Waiters | service done, routing to Cashier | 1 | 2 |
| 2008.6346 | Seize | 127 | Waiters | pulled from queue after waiting 6.1605, service until 2047.5673 | 0 | 3 |
| 2008.6346 | Seize | 125 | Cashier | server free, service 1.7463 until 2010.3809 | 0 | 1 |
| 2010.3809 | Exit | 125 | Cashier | exits; total wait 0.0000, time in system 40.6052 | 0 | 0 |
| 2015.9839 | Move | 124 | Waiters | service done, routing to Cashier | 0 | 2 |
| 2015.9839 | Seize | 124 | Cashier | server free, service 3.7923 until 2019.7762 | 0 | 1 |
| 2019.7762 | Exit | 124 | Cashier | exits; total wait 21.6565, time in system 75.1629 | 0 | 0 |
| 2023.5917 | Move | 126 | Waiters | service done, routing to Cashier | 0 | 1 |
| 2023.5917 | Seize | 126 | Cashier | server free, service 2.7969 until 2026.3886 | 0 | 1 |
| 2026.3886 | Exit | 126 | Cashier | exits; total wait 3.2690, time in system 33.4340 | 0 | 0 |
| 2031.4960 | Arrival | 128 | Host | enters the system | 0 | 0 |
| 2031.4960 | Seize | 128 | Host | server free, service 1.0992 until 2032.5953 | 0 | 1 |
| 2032.5953 | Move | 128 | Host | service done, routing to Waiters | 0 | 0 |
| 2032.5953 | Seize | 128 | Waiters | server free, service 38.9919 until 2071.5872 | 0 | 2 |
| 2046.1566 | Arrival | 129 | Host | enters the system | 0 | 0 |
| 2046.1566 | Seize | 129 | Host | server free, service 0.3762 until 2046.5328 | 0 | 1 |
| 2046.5328 | Move | 129 | Host | service done, routing to Waiters | 0 | 0 |
| 2046.5328 | Seize | 129 | Waiters | server free, service 42.7828 until 2089.3156 | 0 | 3 |
| 2047.5673 | Move | 127 | Waiters | service done, routing to Cashier | 0 | 2 |
| 2047.5673 | Seize | 127 | Cashier | server free, service 3.4367 until 2051.0041 | 0 | 1 |
| 2048.4378 | Arrival | 130 | Host | enters the system | 0 | 0 |
| 2048.4378 | Seize | 130 | Host | server free, service 0.0853 until 2048.5231 | 0 | 1 |
| 2048.5231 | Move | 130 | Host | service done, routing to Waiters | 0 | 0 |
| 2048.5231 | Seize | 130 | Waiters | server free, service 44.0394 until 2092.5626 | 0 | 3 |
| 2051.0041 | Exit | 127 | Cashier | exits; total wait 6.1605, time in system 56.3065 | 0 | 0 |
| 2063.7149 | Arrival | 131 | Host | enters the system | 0 | 0 |
| 2063.7149 | Seize | 131 | Host | server free, service 0.7187 until 2064.4337 | 0 | 1 |
| 2064.4337 | Move | 131 | Host | service done, routing to Waiters | 0 | 0 |
| 2064.4337 | Queue | 131 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 2071.5872 | Move | 128 | Waiters | service done, routing to Cashier | 1 | 2 |
| 2071.5872 | Seize | 131 | Waiters | pulled from queue after waiting 7.1535, service until 2091.6497 | 0 | 3 |
| 2071.5872 | Seize | 128 | Cashier | server free, service 1.3853 until 2072.9725 | 0 | 1 |
| 2071.9458 | Arrival | 132 | Host | enters the system | 0 | 0 |
| 2071.9458 | Seize | 132 | Host | server free, service 1.8620 until 2073.8077 | 0 | 1 |
| 2072.7427 | Arrival | 133 | Host | enters the system | 0 | 1 |
| 2072.7427 | Queue | 133 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 2072.9725 | Exit | 128 | Cashier | exits; total wait 0.0000, time in system 41.4764 | 0 | 0 |
| 2073.8077 | Move | 132 | Host | service done, routing to Waiters | 1 | 0 |
| 2073.8077 | Seize | 133 | Host | pulled from queue after waiting 1.0651, service until 2074.6263 | 0 | 1 |
| 2073.8077 | Queue | 132 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 2074.6263 | Move | 133 | Host | service done, routing to Waiters | 0 | 0 |
| 2074.6263 | Queue | 133 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 2080.4719 | Arrival | 134 | Host | enters the system | 0 | 0 |
| 2080.4719 | Seize | 134 | Host | server free, service 1.1383 until 2081.6102 | 0 | 1 |
| 2081.6102 | Move | 134 | Host | service done, routing to Waiters | 0 | 0 |
| 2081.6102 | Queue | 134 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 2089.3156 | Move | 129 | Waiters | service done, routing to Cashier | 3 | 2 |
| 2089.3156 | Seize | 132 | Waiters | pulled from queue after waiting 15.5079, service until 2126.2287 | 2 | 3 |
| 2089.3156 | Seize | 129 | Cashier | server free, service 2.2958 until 2091.6115 | 0 | 1 |
| 2091.6115 | Exit | 129 | Cashier | exits; total wait 0.0000, time in system 45.4549 | 0 | 0 |
| 2091.6497 | Move | 131 | Waiters | service done, routing to Cashier | 2 | 2 |
| 2091.6497 | Seize | 133 | Waiters | pulled from queue after waiting 17.0234, service until 2131.2873 | 1 | 3 |
| 2091.6497 | Seize | 131 | Cashier | server free, service 2.6336 until 2094.2833 | 0 | 1 |
| 2091.7406 | Arrival | 135 | Host | enters the system | 0 | 0 |
| 2091.7406 | Seize | 135 | Host | server free, service 0.9568 until 2092.6974 | 0 | 1 |
| 2092.5626 | Move | 130 | Waiters | service done, routing to Cashier | 1 | 2 |
| 2092.5626 | Seize | 134 | Waiters | pulled from queue after waiting 10.9524, service until 2131.0396 | 0 | 3 |
| 2092.5626 | Queue | 130 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 2092.6974 | Move | 135 | Host | service done, routing to Waiters | 0 | 0 |
| 2092.6974 | Queue | 135 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 2094.2833 | Exit | 131 | Cashier | exits; total wait 7.1535, time in system 30.5684 | 1 | 0 |
| 2094.2833 | Seize | 130 | Cashier | pulled from queue after waiting 1.7207, service until 2097.7572 | 0 | 1 |
| 2097.7572 | Exit | 130 | Cashier | exits; total wait 1.7207, time in system 49.3194 | 0 | 0 |
| 2126.2287 | Move | 132 | Waiters | service done, routing to Cashier | 1 | 2 |
| 2126.2287 | Seize | 135 | Waiters | pulled from queue after waiting 33.5313, service until 2154.9267 | 0 | 3 |
| 2126.2287 | Seize | 132 | Cashier | server free, service 1.8961 until 2128.1248 | 0 | 1 |
| 2128.1248 | Exit | 132 | Cashier | exits; total wait 15.5079, time in system 56.1790 | 0 | 0 |
| 2131.0396 | Move | 134 | Waiters | service done, routing to Cashier | 0 | 2 |
| 2131.0396 | Seize | 134 | Cashier | server free, service 2.1119 until 2133.1515 | 0 | 1 |
| 2131.2873 | Move | 133 | Waiters | service done, routing to Cashier | 0 | 1 |
| 2131.2873 | Queue | 133 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 2133.1515 | Exit | 134 | Cashier | exits; total wait 10.9524, time in system 52.6796 | 1 | 0 |
| 2133.1515 | Seize | 133 | Cashier | pulled from queue after waiting 1.8642, service until 2135.4456 | 0 | 1 |
| 2135.4456 | Exit | 133 | Cashier | exits; total wait 19.9526, time in system 62.7029 | 0 | 0 |
| 2154.9267 | Move | 135 | Waiters | service done, routing to Cashier | 0 | 0 |
| 2154.9267 | Seize | 135 | Cashier | server free, service 2.6816 until 2157.6083 | 0 | 1 |
| 2157.6083 | Exit | 135 | Cashier | exits; total wait 33.5313, time in system 65.8677 | 0 | 0 |
| 2183.7808 | Arrival | 136 | Host | enters the system | 0 | 0 |
| 2183.7808 | Seize | 136 | Host | server free, service 1.3421 until 2185.1229 | 0 | 1 |
| 2185.1229 | Move | 136 | Host | service done, routing to Waiters | 0 | 0 |
| 2185.1229 | Seize | 136 | Waiters | server free, service 36.5548 until 2221.6777 | 0 | 1 |
| 2221.6777 | Move | 136 | Waiters | service done, routing to Cashier | 0 | 0 |
| 2221.6777 | Seize | 136 | Cashier | server free, service 3.5458 until 2225.2235 | 0 | 1 |
| 2225.2235 | Exit | 136 | Cashier | exits; total wait 0.0000, time in system 41.4427 | 0 | 0 |
| 2259.2567 | Arrival | 137 | Host | enters the system | 0 | 0 |
| 2259.2567 | Seize | 137 | Host | server free, service 1.0102 until 2260.2669 | 0 | 1 |
| 2260.2669 | Move | 137 | Host | service done, routing to Waiters | 0 | 0 |
| 2260.2669 | Seize | 137 | Waiters | server free, service 51.5048 until 2311.7718 | 0 | 1 |
| 2281.0799 | Arrival | 138 | Host | enters the system | 0 | 0 |
| 2281.0799 | Seize | 138 | Host | server free, service 5.3149 until 2286.3949 | 0 | 1 |
| 2285.6674 | Arrival | 139 | Host | enters the system | 0 | 1 |
| 2285.6674 | Queue | 139 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 2286.3949 | Move | 138 | Host | service done, routing to Waiters | 1 | 0 |
| 2286.3949 | Seize | 139 | Host | pulled from queue after waiting 0.7274, service until 2288.9262 | 0 | 1 |
| 2286.3949 | Seize | 138 | Waiters | server free, service 46.7768 until 2333.1716 | 0 | 2 |
| 2288.9262 | Move | 139 | Host | service done, routing to Waiters | 0 | 0 |
| 2288.9262 | Seize | 139 | Waiters | server free, service 37.5686 until 2326.4948 | 0 | 3 |
| 2305.7295 | Arrival | 140 | Host | enters the system | 0 | 0 |
| 2305.7295 | Seize | 140 | Host | server free, service 1.6284 until 2307.3579 | 0 | 1 |
| 2307.3579 | Move | 140 | Host | service done, routing to Waiters | 0 | 0 |
| 2307.3579 | Queue | 140 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 2310.5106 | Arrival | 141 | Host | enters the system | 0 | 0 |
| 2310.5106 | Seize | 141 | Host | server free, service 3.0526 until 2313.5632 | 0 | 1 |
| 2311.7718 | Move | 137 | Waiters | service done, routing to Cashier | 1 | 2 |
| 2311.7718 | Seize | 140 | Waiters | pulled from queue after waiting 4.4138, service until 2338.0750 | 0 | 3 |
| 2311.7718 | Seize | 137 | Cashier | server free, service 2.1388 until 2313.9106 | 0 | 1 |
| 2313.5632 | Move | 141 | Host | service done, routing to Waiters | 0 | 0 |
| 2313.5632 | Queue | 141 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 2313.9106 | Exit | 137 | Cashier | exits; total wait 0.0000, time in system 54.6538 | 0 | 0 |
| 2326.4948 | Move | 139 | Waiters | service done, routing to Cashier | 1 | 2 |
| 2326.4948 | Seize | 141 | Waiters | pulled from queue after waiting 12.9316, service until 2355.2748 | 0 | 3 |
| 2326.4948 | Seize | 139 | Cashier | server free, service 3.1560 until 2329.6507 | 0 | 1 |
| 2329.6507 | Exit | 139 | Cashier | exits; total wait 0.7274, time in system 43.9833 | 0 | 0 |
| 2333.1716 | Move | 138 | Waiters | service done, routing to Cashier | 0 | 2 |
| 2333.1716 | Seize | 138 | Cashier | server free, service 2.9949 until 2336.1665 | 0 | 1 |
| 2336.1665 | Exit | 138 | Cashier | exits; total wait 0.0000, time in system 55.0866 | 0 | 0 |
| 2338.0750 | Move | 140 | Waiters | service done, routing to Cashier | 0 | 1 |
| 2338.0750 | Seize | 140 | Cashier | server free, service 3.3850 until 2341.4600 | 0 | 1 |
| 2341.4600 | Exit | 140 | Cashier | exits; total wait 4.4138, time in system 35.7305 | 0 | 0 |
| 2351.4088 | Arrival | 142 | Host | enters the system | 0 | 0 |
| 2351.4088 | Seize | 142 | Host | server free, service 2.1597 until 2353.5685 | 0 | 1 |
| 2352.2240 | Arrival | 143 | Host | enters the system | 0 | 1 |
| 2352.2240 | Queue | 143 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 2353.5685 | Move | 142 | Host | service done, routing to Waiters | 1 | 0 |
| 2353.5685 | Seize | 143 | Host | pulled from queue after waiting 1.3446, service until 2355.7757 | 0 | 1 |
| 2353.5685 | Seize | 142 | Waiters | server free, service 55.0328 until 2408.6013 | 0 | 2 |
| 2355.1784 | Arrival | 144 | Host | enters the system | 0 | 1 |
| 2355.1784 | Queue | 144 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 2355.2748 | Move | 141 | Waiters | service done, routing to Cashier | 0 | 1 |
| 2355.2748 | Seize | 141 | Cashier | server free, service 2.6920 until 2357.9668 | 0 | 1 |
| 2355.7757 | Move | 143 | Host | service done, routing to Waiters | 1 | 0 |
| 2355.7757 | Seize | 144 | Host | pulled from queue after waiting 0.5972, service until 2362.8507 | 0 | 1 |
| 2355.7757 | Seize | 143 | Waiters | server free, service 35.1693 until 2390.9449 | 0 | 2 |
| 2356.9755 | Arrival | 145 | Host | enters the system | 0 | 1 |
| 2356.9755 | Queue | 145 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 2357.9668 | Exit | 141 | Cashier | exits; total wait 12.9316, time in system 47.4563 | 0 | 0 |
| 2362.8507 | Move | 144 | Host | service done, routing to Waiters | 1 | 0 |
| 2362.8507 | Seize | 145 | Host | pulled from queue after waiting 5.8753, service until 2363.6666 | 0 | 1 |
| 2362.8507 | Seize | 144 | Waiters | server free, service 36.7246 until 2399.5754 | 0 | 3 |
| 2363.6666 | Move | 145 | Host | service done, routing to Waiters | 0 | 0 |
| 2363.6666 | Queue | 145 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 2375.2597 | Arrival | 146 | Host | enters the system | 0 | 0 |
| 2375.2597 | Seize | 146 | Host | server free, service 0.1488 until 2375.4085 | 0 | 1 |
| 2375.4085 | Move | 146 | Host | service done, routing to Waiters | 0 | 0 |
| 2375.4085 | Queue | 146 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 2390.9449 | Move | 143 | Waiters | service done, routing to Cashier | 2 | 2 |
| 2390.9449 | Seize | 145 | Waiters | pulled from queue after waiting 27.2784, service until 2427.5876 | 1 | 3 |
| 2390.9449 | Seize | 143 | Cashier | server free, service 1.0455 until 2391.9904 | 0 | 1 |
| 2391.9904 | Exit | 143 | Cashier | exits; total wait 1.3446, time in system 39.7665 | 0 | 0 |
| 2399.5754 | Move | 144 | Waiters | service done, routing to Cashier | 1 | 2 |
| 2399.5754 | Seize | 146 | Waiters | pulled from queue after waiting 24.1669, service until 2437.9195 | 0 | 3 |
| 2399.5754 | Seize | 144 | Cashier | server free, service 3.1689 until 2402.7443 | 0 | 1 |
| 2402.7443 | Exit | 144 | Cashier | exits; total wait 0.5972, time in system 47.5659 | 0 | 0 |
| 2408.6013 | Move | 142 | Waiters | service done, routing to Cashier | 0 | 2 |
| 2408.6013 | Seize | 142 | Cashier | server free, service 3.4195 until 2412.0208 | 0 | 1 |
| 2412.0208 | Exit | 142 | Cashier | exits; total wait 0.0000, time in system 60.6120 | 0 | 0 |
| 2417.2806 | Arrival | 147 | Host | enters the system | 0 | 0 |
| 2417.2806 | Seize | 147 | Host | server free, service 2.5149 until 2419.7955 | 0 | 1 |
| 2419.7955 | Move | 147 | Host | service done, routing to Waiters | 0 | 0 |
| 2419.7955 | Seize | 147 | Waiters | server free, service 47.0517 until 2466.8472 | 0 | 3 |
| 2423.6608 | Arrival | 148 | Host | enters the system | 0 | 0 |
| 2423.6608 | Seize | 148 | Host | server free, service 0.6857 until 2424.3465 | 0 | 1 |
| 2424.3465 | Move | 148 | Host | service done, routing to Waiters | 0 | 0 |
| 2424.3465 | Queue | 148 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 2427.5876 | Move | 145 | Waiters | service done, routing to Cashier | 1 | 2 |
| 2427.5876 | Seize | 148 | Waiters | pulled from queue after waiting 3.2411, service until 2473.8873 | 0 | 3 |
| 2427.5876 | Seize | 145 | Cashier | server free, service 1.9745 until 2429.5620 | 0 | 1 |
| 2429.5620 | Exit | 145 | Cashier | exits; total wait 33.1536, time in system 72.5865 | 0 | 0 |
| 2437.9195 | Move | 146 | Waiters | service done, routing to Cashier | 0 | 2 |
| 2437.9195 | Seize | 146 | Cashier | server free, service 1.0981 until 2439.0176 | 0 | 1 |
| 2439.0176 | Exit | 146 | Cashier | exits; total wait 24.1669, time in system 63.7579 | 0 | 0 |
| 2460.8281 | Arrival | 149 | Host | enters the system | 0 | 0 |
| 2460.8281 | Seize | 149 | Host | server free, service 0.2298 until 2461.0579 | 0 | 1 |
| 2461.0579 | Move | 149 | Host | service done, routing to Waiters | 0 | 0 |
| 2461.0579 | Seize | 149 | Waiters | server free, service 32.6296 until 2493.6875 | 0 | 3 |
| 2466.8472 | Move | 147 | Waiters | service done, routing to Cashier | 0 | 2 |
| 2466.8472 | Seize | 147 | Cashier | server free, service 1.1032 until 2467.9504 | 0 | 1 |
| 2467.9504 | Exit | 147 | Cashier | exits; total wait 0.0000, time in system 50.6698 | 0 | 0 |
| 2473.8873 | Move | 148 | Waiters | service done, routing to Cashier | 0 | 1 |
| 2473.8873 | Seize | 148 | Cashier | server free, service 2.7974 until 2476.6847 | 0 | 1 |
| 2476.6847 | Exit | 148 | Cashier | exits; total wait 3.2411, time in system 53.0239 | 0 | 0 |
| 2484.4592 | Arrival | 150 | Host | enters the system | 0 | 0 |
| 2484.4592 | Seize | 150 | Host | server free, service 1.0955 until 2485.5546 | 0 | 1 |
| 2485.5546 | Move | 150 | Host | service done, routing to Waiters | 0 | 0 |
| 2485.5546 | Seize | 150 | Waiters | server free, service 31.7539 until 2517.3085 | 0 | 2 |
| 2486.3767 | Arrival | 151 | Host | enters the system | 0 | 0 |
| 2486.3767 | Seize | 151 | Host | server free, service 1.0875 until 2487.4641 | 0 | 1 |
| 2487.4641 | Move | 151 | Host | service done, routing to Waiters | 0 | 0 |
| 2487.4641 | Seize | 151 | Waiters | server free, service 49.6238 until 2537.0879 | 0 | 3 |
| 2493.6875 | Move | 149 | Waiters | service done, routing to Cashier | 0 | 2 |
| 2493.6875 | Seize | 149 | Cashier | server free, service 1.7906 until 2495.4781 | 0 | 1 |
| 2495.4781 | Exit | 149 | Cashier | exits; total wait 0.0000, time in system 34.6500 | 0 | 0 |
| 2502.9097 | Arrival | 152 | Host | enters the system | 0 | 0 |
| 2502.9097 | Seize | 152 | Host | server free, service 0.2053 until 2503.1150 | 0 | 1 |
| 2503.1150 | Move | 152 | Host | service done, routing to Waiters | 0 | 0 |
| 2503.1150 | Seize | 152 | Waiters | server free, service 40.2533 until 2543.3683 | 0 | 3 |
| 2515.8721 | Arrival | 153 | Host | enters the system | 0 | 0 |
| 2515.8721 | Seize | 153 | Host | server free, service 4.0421 until 2519.9142 | 0 | 1 |
| 2517.3085 | Move | 150 | Waiters | service done, routing to Cashier | 0 | 2 |
| 2517.3085 | Seize | 150 | Cashier | server free, service 1.9555 until 2519.2640 | 0 | 1 |
| 2519.2640 | Exit | 150 | Cashier | exits; total wait 0.0000, time in system 34.8049 | 0 | 0 |
| 2519.9142 | Move | 153 | Host | service done, routing to Waiters | 0 | 0 |
| 2519.9142 | Seize | 153 | Waiters | server free, service 47.0219 until 2566.9362 | 0 | 3 |
| 2522.5164 | Arrival | 154 | Host | enters the system | 0 | 0 |
| 2522.5164 | Seize | 154 | Host | server free, service 2.3942 until 2524.9107 | 0 | 1 |
| 2524.9107 | Move | 154 | Host | service done, routing to Waiters | 0 | 0 |
| 2524.9107 | Queue | 154 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 2537.0879 | Move | 151 | Waiters | service done, routing to Cashier | 1 | 2 |
| 2537.0879 | Seize | 154 | Waiters | pulled from queue after waiting 12.1773, service until 2574.3615 | 0 | 3 |
| 2537.0879 | Seize | 151 | Cashier | server free, service 1.7693 until 2538.8572 | 0 | 1 |
| 2538.8572 | Exit | 151 | Cashier | exits; total wait 0.0000, time in system 52.4805 | 0 | 0 |
| 2543.3683 | Move | 152 | Waiters | service done, routing to Cashier | 0 | 2 |
| 2543.3683 | Seize | 152 | Cashier | server free, service 1.1629 until 2544.5312 | 0 | 1 |
| 2544.5312 | Exit | 152 | Cashier | exits; total wait 0.0000, time in system 41.6215 | 0 | 0 |
| 2566.7961 | Arrival | 155 | Host | enters the system | 0 | 0 |
| 2566.7961 | Seize | 155 | Host | server free, service 7.2318 until 2574.0279 | 0 | 1 |
| 2566.9362 | Move | 153 | Waiters | service done, routing to Cashier | 0 | 1 |
| 2566.9362 | Seize | 153 | Cashier | server free, service 2.2362 until 2569.1723 | 0 | 1 |
| 2567.9502 | Arrival | 156 | Host | enters the system | 0 | 1 |
| 2567.9502 | Queue | 156 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 2569.1723 | Exit | 153 | Cashier | exits; total wait 0.0000, time in system 53.3002 | 0 | 0 |
| 2574.0279 | Move | 155 | Host | service done, routing to Waiters | 1 | 0 |
| 2574.0279 | Seize | 156 | Host | pulled from queue after waiting 6.0778, service until 2575.8176 | 0 | 1 |
| 2574.0279 | Seize | 155 | Waiters | server free, service 38.1997 until 2612.2277 | 0 | 2 |
| 2574.3615 | Move | 154 | Waiters | service done, routing to Cashier | 0 | 1 |
| 2574.3615 | Seize | 154 | Cashier | server free, service 3.3395 until 2577.7010 | 0 | 1 |
| 2575.8176 | Move | 156 | Host | service done, routing to Waiters | 0 | 0 |
| 2575.8176 | Seize | 156 | Waiters | server free, service 44.6194 until 2620.4369 | 0 | 2 |
| 2577.7010 | Exit | 154 | Cashier | exits; total wait 12.1773, time in system 55.1846 | 0 | 0 |
| 2578.1029 | Arrival | 157 | Host | enters the system | 0 | 0 |
| 2578.1029 | Seize | 157 | Host | server free, service 0.6565 until 2578.7594 | 0 | 1 |
| 2578.7594 | Move | 157 | Host | service done, routing to Waiters | 0 | 0 |
| 2578.7594 | Seize | 157 | Waiters | server free, service 29.6103 until 2608.3697 | 0 | 3 |
| 2580.2122 | Arrival | 158 | Host | enters the system | 0 | 0 |
| 2580.2122 | Seize | 158 | Host | server free, service 2.0183 until 2582.2306 | 0 | 1 |
| 2582.2306 | Move | 158 | Host | service done, routing to Waiters | 0 | 0 |
| 2582.2306 | Queue | 158 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 2582.5339 | Arrival | 159 | Host | enters the system | 0 | 0 |
| 2582.5339 | Seize | 159 | Host | server free, service 3.5442 until 2586.0782 | 0 | 1 |
| 2586.0782 | Move | 159 | Host | service done, routing to Waiters | 0 | 0 |
| 2586.0782 | Queue | 159 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 2586.3754 | Arrival | 160 | Host | enters the system | 0 | 0 |
| 2586.3754 | Seize | 160 | Host | server free, service 0.6363 until 2587.0117 | 0 | 1 |
| 2587.0117 | Move | 160 | Host | service done, routing to Waiters | 0 | 0 |
| 2587.0117 | Queue | 160 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 2608.3697 | Move | 157 | Waiters | service done, routing to Cashier | 3 | 2 |
| 2608.3697 | Seize | 158 | Waiters | pulled from queue after waiting 26.1391, service until 2652.6310 | 2 | 3 |
| 2608.3697 | Seize | 157 | Cashier | server free, service 1.8090 until 2610.1787 | 0 | 1 |
| 2610.1787 | Exit | 157 | Cashier | exits; total wait 0.0000, time in system 32.0758 | 0 | 0 |
| 2612.2277 | Move | 155 | Waiters | service done, routing to Cashier | 2 | 2 |
| 2612.2277 | Seize | 159 | Waiters | pulled from queue after waiting 26.1495, service until 2637.6274 | 1 | 3 |
| 2612.2277 | Seize | 155 | Cashier | server free, service 3.1869 until 2615.4146 | 0 | 1 |
| 2615.4146 | Exit | 155 | Cashier | exits; total wait 0.0000, time in system 48.6184 | 0 | 0 |
| 2619.2308 | Arrival | 161 | Host | enters the system | 0 | 0 |
| 2619.2308 | Seize | 161 | Host | server free, service 7.8931 until 2627.1239 | 0 | 1 |
| 2620.4369 | Move | 156 | Waiters | service done, routing to Cashier | 1 | 2 |
| 2620.4369 | Seize | 160 | Waiters | pulled from queue after waiting 33.4252, service until 2661.7464 | 0 | 3 |
| 2620.4369 | Seize | 156 | Cashier | server free, service 1.6492 until 2622.0861 | 0 | 1 |
| 2622.0861 | Exit | 156 | Cashier | exits; total wait 6.0778, time in system 54.1360 | 0 | 0 |
| 2626.4761 | Arrival | 162 | Host | enters the system | 0 | 1 |
| 2626.4761 | Queue | 162 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 2627.1239 | Move | 161 | Host | service done, routing to Waiters | 1 | 0 |
| 2627.1239 | Seize | 162 | Host | pulled from queue after waiting 0.6478, service until 2627.3480 | 0 | 1 |
| 2627.1239 | Queue | 161 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 2627.3480 | Move | 162 | Host | service done, routing to Waiters | 0 | 0 |
| 2627.3480 | Queue | 162 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 2635.9673 | Arrival | 163 | Host | enters the system | 0 | 0 |
| 2635.9673 | Seize | 163 | Host | server free, service 0.6595 until 2636.6268 | 0 | 1 |
| 2636.6268 | Move | 163 | Host | service done, routing to Waiters | 0 | 0 |
| 2636.6268 | Queue | 163 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 2637.6274 | Move | 159 | Waiters | service done, routing to Cashier | 3 | 2 |
| 2637.6274 | Seize | 161 | Waiters | pulled from queue after waiting 10.5036, service until 2669.8002 | 2 | 3 |
| 2637.6274 | Seize | 159 | Cashier | server free, service 1.0008 until 2638.6283 | 0 | 1 |
| 2638.6283 | Exit | 159 | Cashier | exits; total wait 26.1495, time in system 56.0943 | 0 | 0 |
| 2652.6310 | Move | 158 | Waiters | service done, routing to Cashier | 2 | 2 |
| 2652.6310 | Seize | 162 | Waiters | pulled from queue after waiting 25.2830, service until 2696.5061 | 1 | 3 |
| 2652.6310 | Seize | 158 | Cashier | server free, service 3.1644 until 2655.7954 | 0 | 1 |
| 2655.7954 | Exit | 158 | Cashier | exits; total wait 26.1391, time in system 75.5832 | 0 | 0 |
| 2661.7464 | Move | 160 | Waiters | service done, routing to Cashier | 1 | 2 |
| 2661.7464 | Seize | 163 | Waiters | pulled from queue after waiting 25.1197, service until 2702.1955 | 0 | 3 |
| 2661.7464 | Seize | 160 | Cashier | server free, service 1.4904 until 2663.2368 | 0 | 1 |
| 2663.2368 | Exit | 160 | Cashier | exits; total wait 33.4252, time in system 76.8615 | 0 | 0 |
| 2669.8002 | Move | 161 | Waiters | service done, routing to Cashier | 0 | 2 |
| 2669.8002 | Seize | 161 | Cashier | server free, service 2.6645 until 2672.4648 | 0 | 1 |
| 2672.4648 | Exit | 161 | Cashier | exits; total wait 10.5036, time in system 53.2339 | 0 | 0 |
| 2696.5061 | Move | 162 | Waiters | service done, routing to Cashier | 0 | 1 |
| 2696.5061 | Seize | 162 | Cashier | server free, service 1.9223 until 2698.4285 | 0 | 1 |
| 2698.0426 | Arrival | 164 | Host | enters the system | 0 | 0 |
| 2698.0426 | Seize | 164 | Host | server free, service 3.0239 until 2701.0665 | 0 | 1 |
| 2698.4285 | Exit | 162 | Cashier | exits; total wait 25.9308, time in system 71.9524 | 0 | 0 |
| 2701.0665 | Move | 164 | Host | service done, routing to Waiters | 0 | 0 |
| 2701.0665 | Seize | 164 | Waiters | server free, service 24.6897 until 2725.7562 | 0 | 2 |
| 2702.1955 | Move | 163 | Waiters | service done, routing to Cashier | 0 | 1 |
| 2702.1955 | Seize | 163 | Cashier | server free, service 1.3916 until 2703.5872 | 0 | 1 |
| 2703.5872 | Exit | 163 | Cashier | exits; total wait 25.1197, time in system 67.6199 | 0 | 0 |
| 2725.7562 | Move | 164 | Waiters | service done, routing to Cashier | 0 | 0 |
| 2725.7562 | Seize | 164 | Cashier | server free, service 1.4715 until 2727.2278 | 0 | 1 |
| 2727.2278 | Exit | 164 | Cashier | exits; total wait 0.0000, time in system 29.1852 | 0 | 0 |
| 2755.4936 | Arrival | 165 | Host | enters the system | 0 | 0 |
| 2755.4936 | Seize | 165 | Host | server free, service 0.0436 until 2755.5373 | 0 | 1 |
| 2755.5373 | Move | 165 | Host | service done, routing to Waiters | 0 | 0 |
| 2755.5373 | Seize | 165 | Waiters | server free, service 28.9278 until 2784.4651 | 0 | 1 |
| 2761.9813 | Arrival | 166 | Host | enters the system | 0 | 0 |
| 2761.9813 | Seize | 166 | Host | server free, service 3.2274 until 2765.2087 | 0 | 1 |
| 2765.2087 | Move | 166 | Host | service done, routing to Waiters | 0 | 0 |
| 2765.2087 | Seize | 166 | Waiters | server free, service 37.8078 until 2803.0165 | 0 | 2 |
| 2765.6958 | Arrival | 167 | Host | enters the system | 0 | 0 |
| 2765.6958 | Seize | 167 | Host | server free, service 0.2767 until 2765.9724 | 0 | 1 |
| 2765.9724 | Move | 167 | Host | service done, routing to Waiters | 0 | 0 |
| 2765.9724 | Seize | 167 | Waiters | server free, service 35.8332 until 2801.8056 | 0 | 3 |
| 2776.5341 | Arrival | 168 | Host | enters the system | 0 | 0 |
| 2776.5341 | Seize | 168 | Host | server free, service 0.2546 until 2776.7887 | 0 | 1 |
| 2776.7887 | Move | 168 | Host | service done, routing to Waiters | 0 | 0 |
| 2776.7887 | Queue | 168 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 2777.4309 | Arrival | 169 | Host | enters the system | 0 | 0 |
| 2777.4309 | Seize | 169 | Host | server free, service 0.2760 until 2777.7070 | 0 | 1 |
| 2777.7070 | Move | 169 | Host | service done, routing to Waiters | 0 | 0 |
| 2777.7070 | Queue | 169 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 2784.4651 | Move | 165 | Waiters | service done, routing to Cashier | 2 | 2 |
| 2784.4651 | Seize | 168 | Waiters | pulled from queue after waiting 7.6764, service until 2814.6305 | 1 | 3 |
| 2784.4651 | Seize | 165 | Cashier | server free, service 3.9375 until 2788.4026 | 0 | 1 |
| 2788.4026 | Exit | 165 | Cashier | exits; total wait 0.0000, time in system 32.9089 | 0 | 0 |
| 2792.3169 | Arrival | 170 | Host | enters the system | 0 | 0 |
| 2792.3169 | Seize | 170 | Host | server free, service 1.4331 until 2793.7500 | 0 | 1 |
| 2793.7500 | Move | 170 | Host | service done, routing to Waiters | 0 | 0 |
| 2793.7500 | Queue | 170 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 2801.8056 | Move | 167 | Waiters | service done, routing to Cashier | 2 | 2 |
| 2801.8056 | Seize | 169 | Waiters | pulled from queue after waiting 24.0987, service until 2839.7377 | 1 | 3 |
| 2801.8056 | Seize | 167 | Cashier | server free, service 2.7368 until 2804.5425 | 0 | 1 |
| 2803.0165 | Move | 166 | Waiters | service done, routing to Cashier | 1 | 2 |
| 2803.0165 | Seize | 170 | Waiters | pulled from queue after waiting 9.2665, service until 2830.3695 | 0 | 3 |
| 2803.0165 | Queue | 166 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 2804.5425 | Exit | 167 | Cashier | exits; total wait 0.0000, time in system 38.8467 | 1 | 0 |
| 2804.5425 | Seize | 166 | Cashier | pulled from queue after waiting 1.5260, service until 2807.5096 | 0 | 1 |
| 2807.5096 | Exit | 166 | Cashier | exits; total wait 1.5260, time in system 45.5283 | 0 | 0 |
| 2814.6305 | Move | 168 | Waiters | service done, routing to Cashier | 0 | 2 |
| 2814.6305 | Seize | 168 | Cashier | server free, service 3.3290 until 2817.9596 | 0 | 1 |
| 2817.9596 | Exit | 168 | Cashier | exits; total wait 7.6764, time in system 41.4255 | 0 | 0 |
| 2818.9816 | Arrival | 171 | Host | enters the system | 0 | 0 |
| 2818.9816 | Seize | 171 | Host | server free, service 0.0697 until 2819.0513 | 0 | 1 |
| 2819.0513 | Move | 171 | Host | service done, routing to Waiters | 0 | 0 |
| 2819.0513 | Seize | 171 | Waiters | server free, service 42.6888 until 2861.7401 | 0 | 3 |
| 2820.5733 | Arrival | 172 | Host | enters the system | 0 | 0 |
| 2820.5733 | Seize | 172 | Host | server free, service 8.8816 until 2829.4550 | 0 | 1 |
| 2829.4550 | Move | 172 | Host | service done, routing to Waiters | 0 | 0 |
| 2829.4550 | Queue | 172 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 2830.3695 | Move | 170 | Waiters | service done, routing to Cashier | 1 | 2 |
| 2830.3695 | Seize | 172 | Waiters | pulled from queue after waiting 0.9145, service until 2878.1096 | 0 | 3 |
| 2830.3695 | Seize | 170 | Cashier | server free, service 1.6393 until 2832.0088 | 0 | 1 |
| 2832.0088 | Exit | 170 | Cashier | exits; total wait 9.2665, time in system 39.6919 | 0 | 0 |
| 2839.7377 | Move | 169 | Waiters | service done, routing to Cashier | 0 | 2 |
| 2839.7377 | Seize | 169 | Cashier | server free, service 2.5688 until 2842.3065 | 0 | 1 |
| 2842.3065 | Exit | 169 | Cashier | exits; total wait 24.0987, time in system 64.8755 | 0 | 0 |
| 2847.0940 | Arrival | 173 | Host | enters the system | 0 | 0 |
| 2847.0940 | Seize | 173 | Host | server free, service 3.3231 until 2850.4172 | 0 | 1 |
| 2850.4172 | Move | 173 | Host | service done, routing to Waiters | 0 | 0 |
| 2850.4172 | Seize | 173 | Waiters | server free, service 37.0215 until 2887.4387 | 0 | 3 |
| 2861.7401 | Move | 171 | Waiters | service done, routing to Cashier | 0 | 2 |
| 2861.7401 | Seize | 171 | Cashier | server free, service 1.2561 until 2862.9962 | 0 | 1 |
| 2862.9962 | Exit | 171 | Cashier | exits; total wait 0.0000, time in system 44.0146 | 0 | 0 |
| 2866.8083 | Arrival | 174 | Host | enters the system | 0 | 0 |
| 2866.8083 | Seize | 174 | Host | server free, service 0.5301 until 2867.3384 | 0 | 1 |
| 2867.3384 | Move | 174 | Host | service done, routing to Waiters | 0 | 0 |
| 2867.3384 | Seize | 174 | Waiters | server free, service 47.8865 until 2915.2248 | 0 | 3 |
| 2878.1096 | Move | 172 | Waiters | service done, routing to Cashier | 0 | 2 |
| 2878.1096 | Seize | 172 | Cashier | server free, service 2.7234 until 2880.8329 | 0 | 1 |
| 2880.8329 | Exit | 172 | Cashier | exits; total wait 0.9145, time in system 60.2596 | 0 | 0 |
| 2885.9032 | Arrival | 175 | Host | enters the system | 0 | 0 |
| 2885.9032 | Seize | 175 | Host | server free, service 3.0035 until 2888.9067 | 0 | 1 |
| 2887.4387 | Move | 173 | Waiters | service done, routing to Cashier | 0 | 1 |
| 2887.4387 | Seize | 173 | Cashier | server free, service 1.8561 until 2889.2948 | 0 | 1 |
| 2888.0250 | Arrival | 176 | Host | enters the system | 0 | 1 |
| 2888.0250 | Queue | 176 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 2888.9067 | Move | 175 | Host | service done, routing to Waiters | 1 | 0 |
| 2888.9067 | Seize | 176 | Host | pulled from queue after waiting 0.8817, service until 2888.9572 | 0 | 1 |
| 2888.9067 | Seize | 175 | Waiters | server free, service 34.2714 until 2923.1780 | 0 | 2 |
| 2888.9572 | Move | 176 | Host | service done, routing to Waiters | 0 | 0 |
| 2888.9572 | Seize | 176 | Waiters | server free, service 53.1962 until 2942.1534 | 0 | 3 |
| 2889.2948 | Exit | 173 | Cashier | exits; total wait 0.0000, time in system 42.2008 | 0 | 0 |
| 2903.2970 | Arrival | 177 | Host | enters the system | 0 | 0 |
| 2903.2970 | Seize | 177 | Host | server free, service 0.6152 until 2903.9122 | 0 | 1 |
| 2903.9122 | Move | 177 | Host | service done, routing to Waiters | 0 | 0 |
| 2903.9122 | Queue | 177 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 2915.2248 | Move | 174 | Waiters | service done, routing to Cashier | 1 | 2 |
| 2915.2248 | Seize | 177 | Waiters | pulled from queue after waiting 11.3126, service until 2949.1637 | 0 | 3 |
| 2915.2248 | Seize | 174 | Cashier | server free, service 1.4308 until 2916.6556 | 0 | 1 |
| 2916.6556 | Exit | 174 | Cashier | exits; total wait 0.0000, time in system 49.8473 | 0 | 0 |
| 2923.1780 | Move | 175 | Waiters | service done, routing to Cashier | 0 | 2 |
| 2923.1780 | Seize | 175 | Cashier | server free, service 1.1066 until 2924.2847 | 0 | 1 |
| 2924.2847 | Exit | 175 | Cashier | exits; total wait 0.0000, time in system 38.3815 | 0 | 0 |
| 2942.1534 | Move | 176 | Waiters | service done, routing to Cashier | 0 | 1 |
| 2942.1534 | Seize | 176 | Cashier | server free, service 3.0087 until 2945.1621 | 0 | 1 |
| 2942.2528 | Arrival | 178 | Host | enters the system | 0 | 0 |
| 2942.2528 | Seize | 178 | Host | server free, service 2.0088 until 2944.2616 | 0 | 1 |
| 2944.2616 | Move | 178 | Host | service done, routing to Waiters | 0 | 0 |
| 2944.2616 | Seize | 178 | Waiters | server free, service 28.0446 until 2972.3062 | 0 | 2 |
| 2944.8442 | Arrival | 179 | Host | enters the system | 0 | 0 |
| 2944.8442 | Seize | 179 | Host | server free, service 2.2039 until 2947.0480 | 0 | 1 |
| 2945.1621 | Exit | 176 | Cashier | exits; total wait 0.8817, time in system 57.1371 | 0 | 0 |
| 2947.0480 | Move | 179 | Host | service done, routing to Waiters | 0 | 0 |
| 2947.0480 | Seize | 179 | Waiters | server free, service 54.3144 until 3001.3624 | 0 | 3 |
| 2949.1637 | Move | 177 | Waiters | service done, routing to Cashier | 0 | 2 |
| 2949.1637 | Seize | 177 | Cashier | server free, service 3.7740 until 2952.9378 | 0 | 1 |
| 2952.1706 | Arrival | 180 | Host | enters the system | 0 | 0 |
| 2952.1706 | Seize | 180 | Host | server free, service 0.9331 until 2953.1037 | 0 | 1 |
| 2952.9378 | Exit | 177 | Cashier | exits; total wait 11.3126, time in system 49.6408 | 0 | 0 |
| 2953.1037 | Move | 180 | Host | service done, routing to Waiters | 0 | 0 |
| 2953.1037 | Seize | 180 | Waiters | server free, service 36.5767 until 2989.6805 | 0 | 3 |
| 2957.4147 | Arrival | 181 | Host | enters the system | 0 | 0 |
| 2957.4147 | Seize | 181 | Host | server free, service 3.3715 until 2960.7862 | 0 | 1 |
| 2960.7862 | Move | 181 | Host | service done, routing to Waiters | 0 | 0 |
| 2960.7862 | Queue | 181 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 2962.9619 | Arrival | 182 | Host | enters the system | 0 | 0 |
| 2962.9619 | Seize | 182 | Host | server free, service 0.4116 until 2963.3735 | 0 | 1 |
| 2963.3735 | Move | 182 | Host | service done, routing to Waiters | 0 | 0 |
| 2963.3735 | Queue | 182 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 2972.3062 | Move | 178 | Waiters | service done, routing to Cashier | 2 | 2 |
| 2972.3062 | Seize | 181 | Waiters | pulled from queue after waiting 11.5199, service until 3006.5406 | 1 | 3 |
| 2972.3062 | Seize | 178 | Cashier | server free, service 1.1992 until 2973.5054 | 0 | 1 |
| 2973.5054 | Exit | 178 | Cashier | exits; total wait 0.0000, time in system 31.2525 | 0 | 0 |
| 2989.6805 | Move | 180 | Waiters | service done, routing to Cashier | 1 | 2 |
| 2989.6805 | Seize | 182 | Waiters | pulled from queue after waiting 26.3069, service until 3037.5392 | 0 | 3 |
| 2989.6805 | Seize | 180 | Cashier | server free, service 2.3358 until 2992.0163 | 0 | 1 |
| 2992.0163 | Exit | 180 | Cashier | exits; total wait 0.0000, time in system 39.8456 | 0 | 0 |
| 2993.0174 | Arrival | 183 | Host | enters the system | 0 | 0 |
| 2993.0174 | Seize | 183 | Host | server free, service 1.5474 until 2994.5648 | 0 | 1 |
| 2994.5648 | Move | 183 | Host | service done, routing to Waiters | 0 | 0 |
| 2994.5648 | Queue | 183 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 3001.3624 | Move | 179 | Waiters | service done, routing to Cashier | 1 | 2 |
| 3001.3624 | Seize | 183 | Waiters | pulled from queue after waiting 6.7976, service until 3039.5039 | 0 | 3 |
| 3001.3624 | Seize | 179 | Cashier | server free, service 3.0575 until 3004.4199 | 0 | 1 |
| 3004.4199 | Exit | 179 | Cashier | exits; total wait 0.0000, time in system 59.5758 | 0 | 0 |
| 3006.5406 | Move | 181 | Waiters | service done, routing to Cashier | 0 | 2 |
| 3006.5406 | Seize | 181 | Cashier | server free, service 1.0026 until 3007.5432 | 0 | 1 |
| 3007.5432 | Exit | 181 | Cashier | exits; total wait 11.5199, time in system 50.1285 | 0 | 0 |
| 3009.0468 | Arrival | 184 | Host | enters the system | 0 | 0 |
| 3009.0468 | Seize | 184 | Host | server free, service 2.9231 until 3011.9700 | 0 | 1 |
| 3011.9700 | Move | 184 | Host | service done, routing to Waiters | 0 | 0 |
| 3011.9700 | Seize | 184 | Waiters | server free, service 37.5252 until 3049.4951 | 0 | 3 |
| 3021.7796 | Arrival | 185 | Host | enters the system | 0 | 0 |
| 3021.7796 | Seize | 185 | Host | server free, service 2.5709 until 3024.3504 | 0 | 1 |
| 3023.5675 | Arrival | 186 | Host | enters the system | 0 | 1 |
| 3023.5675 | Queue | 186 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 3024.3504 | Move | 185 | Host | service done, routing to Waiters | 1 | 0 |
| 3024.3504 | Seize | 186 | Host | pulled from queue after waiting 0.7829, service until 3024.9748 | 0 | 1 |
| 3024.3504 | Queue | 185 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 3024.9748 | Move | 186 | Host | service done, routing to Waiters | 0 | 0 |
| 3024.9748 | Queue | 186 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 3037.5392 | Move | 182 | Waiters | service done, routing to Cashier | 2 | 2 |
| 3037.5392 | Seize | 185 | Waiters | pulled from queue after waiting 13.1887, service until 3080.8154 | 1 | 3 |
| 3037.5392 | Seize | 182 | Cashier | server free, service 3.3224 until 3040.8616 | 0 | 1 |
| 3039.5039 | Move | 183 | Waiters | service done, routing to Cashier | 1 | 2 |
| 3039.5039 | Seize | 186 | Waiters | pulled from queue after waiting 14.5291, service until 3077.6061 | 0 | 3 |
| 3039.5039 | Queue | 183 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 3040.8616 | Exit | 182 | Cashier | exits; total wait 26.3069, time in system 77.8997 | 1 | 0 |
| 3040.8616 | Seize | 183 | Cashier | pulled from queue after waiting 1.3577, service until 3043.4686 | 0 | 1 |
| 3043.4686 | Exit | 183 | Cashier | exits; total wait 8.1553, time in system 50.4512 | 0 | 0 |
| 3049.4951 | Move | 184 | Waiters | service done, routing to Cashier | 0 | 2 |
| 3049.4951 | Seize | 184 | Cashier | server free, service 3.7098 until 3053.2049 | 0 | 1 |
| 3053.2049 | Exit | 184 | Cashier | exits; total wait 0.0000, time in system 44.1581 | 0 | 0 |
| 3066.2915 | Arrival | 187 | Host | enters the system | 0 | 0 |
| 3066.2915 | Seize | 187 | Host | server free, service 1.3830 until 3067.6745 | 0 | 1 |
| 3067.6745 | Move | 187 | Host | service done, routing to Waiters | 0 | 0 |
| 3067.6745 | Seize | 187 | Waiters | server free, service 32.4083 until 3100.0828 | 0 | 3 |
| 3073.8923 | Arrival | 188 | Host | enters the system | 0 | 0 |
| 3073.8923 | Seize | 188 | Host | server free, service 3.0673 until 3076.9596 | 0 | 1 |
| 3076.9596 | Move | 188 | Host | service done, routing to Waiters | 0 | 0 |
| 3076.9596 | Queue | 188 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 3077.6061 | Move | 186 | Waiters | service done, routing to Cashier | 1 | 2 |
| 3077.6061 | Seize | 188 | Waiters | pulled from queue after waiting 0.6465, service until 3112.7321 | 0 | 3 |
| 3077.6061 | Seize | 186 | Cashier | server free, service 3.5058 until 3081.1119 | 0 | 1 |
| 3080.8154 | Move | 185 | Waiters | service done, routing to Cashier | 0 | 2 |
| 3080.8154 | Queue | 185 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 3081.1119 | Exit | 186 | Cashier | exits; total wait 15.3120, time in system 57.5444 | 1 | 0 |
| 3081.1119 | Seize | 185 | Cashier | pulled from queue after waiting 0.2965, service until 3084.5540 | 0 | 1 |
| 3084.5540 | Exit | 185 | Cashier | exits; total wait 13.4852, time in system 62.7744 | 0 | 0 |
| 3091.0422 | Arrival | 189 | Host | enters the system | 0 | 0 |
| 3091.0422 | Seize | 189 | Host | server free, service 0.8047 until 3091.8469 | 0 | 1 |
| 3091.8469 | Move | 189 | Host | service done, routing to Waiters | 0 | 0 |
| 3091.8469 | Seize | 189 | Waiters | server free, service 26.3284 until 3118.1753 | 0 | 3 |
| 3100.0828 | Move | 187 | Waiters | service done, routing to Cashier | 0 | 2 |
| 3100.0828 | Seize | 187 | Cashier | server free, service 1.5065 until 3101.5892 | 0 | 1 |
| 3101.5892 | Exit | 187 | Cashier | exits; total wait 0.0000, time in system 35.2978 | 0 | 0 |
| 3106.0841 | Arrival | 190 | Host | enters the system | 0 | 0 |
| 3106.0841 | Seize | 190 | Host | server free, service 0.7293 until 3106.8134 | 0 | 1 |
| 3106.8134 | Move | 190 | Host | service done, routing to Waiters | 0 | 0 |
| 3106.8134 | Seize | 190 | Waiters | server free, service 32.0788 until 3138.8922 | 0 | 3 |
| 3112.7321 | Move | 188 | Waiters | service done, routing to Cashier | 0 | 2 |
| 3112.7321 | Seize | 188 | Cashier | server free, service 3.5396 until 3116.2717 | 0 | 1 |
| 3116.2717 | Exit | 188 | Cashier | exits; total wait 0.6465, time in system 42.3794 | 0 | 0 |
| 3118.1753 | Move | 189 | Waiters | service done, routing to Cashier | 0 | 1 |
| 3118.1753 | Seize | 189 | Cashier | server free, service 3.7349 until 3121.9102 | 0 | 1 |
| 3121.9102 | Exit | 189 | Cashier | exits; total wait 0.0000, time in system 30.8680 | 0 | 0 |
| 3123.0730 | Arrival | 191 | Host | enters the system | 0 | 0 |
| 3123.0730 | Seize | 191 | Host | server free, service 0.4866 until 3123.5596 | 0 | 1 |
| 3123.5596 | Move | 191 | Host | service done, routing to Waiters | 0 | 0 |
| 3123.5596 | Seize | 191 | Waiters | server free, service 37.5184 until 3161.0780 | 0 | 2 |
| 3135.7194 | Arrival | 192 | Host | enters the system | 0 | 0 |
| 3135.7194 | Seize | 192 | Host | server free, service 1.5113 until 3137.2306 | 0 | 1 |
| 3137.2306 | Move | 192 | Host | service done, routing to Waiters | 0 | 0 |
| 3137.2306 | Seize | 192 | Waiters | server free, service 50.9073 until 3188.1379 | 0 | 3 |
| 3137.4279 | Arrival | 193 | Host | enters the system | 0 | 0 |
| 3137.4279 | Seize | 193 | Host | server free, service 0.6014 until 3138.0293 | 0 | 1 |
| 3138.0293 | Move | 193 | Host | service done, routing to Waiters | 0 | 0 |
| 3138.0293 | Queue | 193 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 3138.8922 | Move | 190 | Waiters | service done, routing to Cashier | 1 | 2 |
| 3138.8922 | Seize | 193 | Waiters | pulled from queue after waiting 0.8629, service until 3173.9838 | 0 | 3 |
| 3138.8922 | Seize | 190 | Cashier | server free, service 1.7713 until 3140.6635 | 0 | 1 |
| 3140.6635 | Exit | 190 | Cashier | exits; total wait 0.0000, time in system 34.5794 | 0 | 0 |
| 3145.2695 | Arrival | 194 | Host | enters the system | 0 | 0 |
| 3145.2695 | Seize | 194 | Host | server free, service 1.1583 until 3146.4278 | 0 | 1 |
| 3146.4278 | Move | 194 | Host | service done, routing to Waiters | 0 | 0 |
| 3146.4278 | Queue | 194 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 3158.3860 | Arrival | 195 | Host | enters the system | 0 | 0 |
| 3158.3860 | Seize | 195 | Host | server free, service 4.2759 until 3162.6619 | 0 | 1 |
| 3161.0780 | Move | 191 | Waiters | service done, routing to Cashier | 1 | 2 |
| 3161.0780 | Seize | 194 | Waiters | pulled from queue after waiting 14.6502, service until 3186.4702 | 0 | 3 |
| 3161.0780 | Seize | 191 | Cashier | server free, service 3.1903 until 3164.2684 | 0 | 1 |
| 3162.4183 | Arrival | 196 | Host | enters the system | 0 | 1 |
| 3162.4183 | Queue | 196 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 3162.6619 | Move | 195 | Host | service done, routing to Waiters | 1 | 0 |
| 3162.6619 | Seize | 196 | Host | pulled from queue after waiting 0.2436, service until 3163.3498 | 0 | 1 |
| 3162.6619 | Queue | 195 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 3163.3498 | Move | 196 | Host | service done, routing to Waiters | 0 | 0 |
| 3163.3498 | Queue | 196 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 3164.2684 | Exit | 191 | Cashier | exits; total wait 0.0000, time in system 41.1953 | 0 | 0 |
| 3173.9838 | Move | 193 | Waiters | service done, routing to Cashier | 2 | 2 |
| 3173.9838 | Seize | 195 | Waiters | pulled from queue after waiting 11.3219, service until 3204.0393 | 1 | 3 |
| 3173.9838 | Seize | 193 | Cashier | server free, service 2.0828 until 3176.0666 | 0 | 1 |
| 3176.0666 | Exit | 193 | Cashier | exits; total wait 0.8629, time in system 38.6388 | 0 | 0 |
| 3185.1736 | Arrival | 197 | Host | enters the system | 0 | 0 |
| 3185.1736 | Seize | 197 | Host | server free, service 1.1405 until 3186.3141 | 0 | 1 |
| 3186.3141 | Move | 197 | Host | service done, routing to Waiters | 0 | 0 |
| 3186.3141 | Queue | 197 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 3186.4702 | Move | 194 | Waiters | service done, routing to Cashier | 2 | 2 |
| 3186.4702 | Seize | 196 | Waiters | pulled from queue after waiting 23.1204, service until 3229.6515 | 1 | 3 |
| 3186.4702 | Seize | 194 | Cashier | server free, service 2.8251 until 3189.2953 | 0 | 1 |
| 3188.1379 | Move | 192 | Waiters | service done, routing to Cashier | 1 | 2 |
| 3188.1379 | Seize | 197 | Waiters | pulled from queue after waiting 1.8238, service until 3215.9770 | 0 | 3 |
| 3188.1379 | Queue | 192 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 3189.2953 | Exit | 194 | Cashier | exits; total wait 14.6502, time in system 44.0258 | 1 | 0 |
| 3189.2953 | Seize | 192 | Cashier | pulled from queue after waiting 1.1574, service until 3191.8214 | 0 | 1 |
| 3191.8214 | Exit | 192 | Cashier | exits; total wait 1.1574, time in system 56.1020 | 0 | 0 |
| 3204.0393 | Move | 195 | Waiters | service done, routing to Cashier | 0 | 2 |
| 3204.0393 | Seize | 195 | Cashier | server free, service 2.8936 until 3206.9329 | 0 | 1 |
| 3206.9329 | Exit | 195 | Cashier | exits; total wait 11.3219, time in system 48.5469 | 0 | 0 |
| 3210.8149 | Arrival | 198 | Host | enters the system | 0 | 0 |
| 3210.8149 | Seize | 198 | Host | server free, service 0.2134 until 3211.0283 | 0 | 1 |
| 3211.0283 | Move | 198 | Host | service done, routing to Waiters | 0 | 0 |
| 3211.0283 | Seize | 198 | Waiters | server free, service 44.5537 until 3255.5820 | 0 | 3 |
| 3215.7491 | Arrival | 199 | Host | enters the system | 0 | 0 |
| 3215.7491 | Seize | 199 | Host | server free, service 3.0369 until 3218.7861 | 0 | 1 |
| 3215.9770 | Move | 197 | Waiters | service done, routing to Cashier | 0 | 2 |
| 3215.9770 | Seize | 197 | Cashier | server free, service 2.8594 until 3218.8364 | 0 | 1 |
| 3217.0833 | Arrival | 200 | Host | enters the system | 0 | 1 |
| 3217.0833 | Queue | 200 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 3218.7861 | Move | 199 | Host | service done, routing to Waiters | 1 | 0 |
| 3218.7861 | Seize | 200 | Host | pulled from queue after waiting 1.7028, service until 3219.2465 | 0 | 1 |
| 3218.7861 | Seize | 199 | Waiters | server free, service 33.8217 until 3252.6078 | 0 | 3 |
| 3218.8364 | Exit | 197 | Cashier | exits; total wait 1.8238, time in system 33.6629 | 0 | 0 |
| 3219.2465 | Move | 200 | Host | service done, routing to Waiters | 0 | 0 |
| 3219.2465 | Queue | 200 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 3229.6515 | Move | 196 | Waiters | service done, routing to Cashier | 1 | 2 |
| 3229.6515 | Seize | 200 | Waiters | pulled from queue after waiting 10.4050, service until 3263.0516 | 0 | 3 |
| 3229.6515 | Seize | 196 | Cashier | server free, service 2.8838 until 3232.5353 | 0 | 1 |
| 3229.9450 | Arrival | 201 | Host | enters the system | 0 | 0 |
| 3229.9450 | Seize | 201 | Host | server free, service 1.0934 until 3231.0383 | 0 | 1 |
| 3231.0383 | Move | 201 | Host | service done, routing to Waiters | 0 | 0 |
| 3231.0383 | Queue | 201 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 3232.5353 | Exit | 196 | Cashier | exits; total wait 23.3641, time in system 70.1169 | 0 | 0 |
| 3239.9132 | Arrival | 202 | Host | enters the system | 0 | 0 |
| 3239.9132 | Seize | 202 | Host | server free, service 3.6014 until 3243.5146 | 0 | 1 |
| 3243.5146 | Move | 202 | Host | service done, routing to Waiters | 0 | 0 |
| 3243.5146 | Queue | 202 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 3252.6078 | Move | 199 | Waiters | service done, routing to Cashier | 2 | 2 |
| 3252.6078 | Seize | 201 | Waiters | pulled from queue after waiting 21.5694, service until 3278.5942 | 1 | 3 |
| 3252.6078 | Seize | 199 | Cashier | server free, service 3.1855 until 3255.7933 | 0 | 1 |
| 3252.8252 | Arrival | 203 | Host | enters the system | 0 | 0 |
| 3252.8252 | Seize | 203 | Host | server free, service 0.5119 until 3253.3371 | 0 | 1 |
| 3253.3371 | Move | 203 | Host | service done, routing to Waiters | 0 | 0 |
| 3253.3371 | Queue | 203 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 3255.5820 | Move | 198 | Waiters | service done, routing to Cashier | 2 | 2 |
| 3255.5820 | Seize | 202 | Waiters | pulled from queue after waiting 12.0674, service until 3293.4581 | 1 | 3 |
| 3255.5820 | Queue | 198 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 3255.7933 | Exit | 199 | Cashier | exits; total wait 0.0000, time in system 40.0441 | 1 | 0 |
| 3255.7933 | Seize | 198 | Cashier | pulled from queue after waiting 0.2112, service until 3257.3330 | 0 | 1 |
| 3257.3330 | Exit | 198 | Cashier | exits; total wait 0.2112, time in system 46.5181 | 0 | 0 |
| 3258.2502 | Arrival | 204 | Host | enters the system | 0 | 0 |
| 3258.2502 | Seize | 204 | Host | server free, service 0.7472 until 3258.9974 | 0 | 1 |
| 3258.9974 | Move | 204 | Host | service done, routing to Waiters | 0 | 0 |
| 3258.9974 | Queue | 204 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 3263.0516 | Move | 200 | Waiters | service done, routing to Cashier | 2 | 2 |
| 3263.0516 | Seize | 203 | Waiters | pulled from queue after waiting 9.7145, service until 3296.7244 | 1 | 3 |
| 3263.0516 | Seize | 200 | Cashier | server free, service 3.8977 until 3266.9493 | 0 | 1 |
| 3266.9493 | Exit | 200 | Cashier | exits; total wait 12.1078, time in system 49.8661 | 0 | 0 |
| 3276.4178 | Arrival | 205 | Host | enters the system | 0 | 0 |
| 3276.4178 | Seize | 205 | Host | server free, service 0.2489 until 3276.6667 | 0 | 1 |
| 3276.6667 | Move | 205 | Host | service done, routing to Waiters | 0 | 0 |
| 3276.6667 | Queue | 205 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 3278.5942 | Move | 201 | Waiters | service done, routing to Cashier | 2 | 2 |
| 3278.5942 | Seize | 204 | Waiters | pulled from queue after waiting 19.5969, service until 3305.3887 | 1 | 3 |
| 3278.5942 | Seize | 201 | Cashier | server free, service 3.1799 until 3281.7741 | 0 | 1 |
| 3281.7741 | Exit | 201 | Cashier | exits; total wait 21.5694, time in system 51.8291 | 0 | 0 |
| 3281.9630 | Arrival | 206 | Host | enters the system | 0 | 0 |
| 3281.9630 | Seize | 206 | Host | server free, service 1.0242 until 3282.9872 | 0 | 1 |
| 3282.9872 | Move | 206 | Host | service done, routing to Waiters | 0 | 0 |
| 3282.9872 | Queue | 206 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 3285.6897 | Arrival | 207 | Host | enters the system | 0 | 0 |
| 3285.6897 | Seize | 207 | Host | server free, service 3.6665 until 3289.3562 | 0 | 1 |
| 3289.3562 | Move | 207 | Host | service done, routing to Waiters | 0 | 0 |
| 3289.3562 | Queue | 207 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 3293.4581 | Move | 202 | Waiters | service done, routing to Cashier | 3 | 2 |
| 3293.4581 | Seize | 205 | Waiters | pulled from queue after waiting 16.7914, service until 3324.9646 | 2 | 3 |
| 3293.4581 | Seize | 202 | Cashier | server free, service 2.3044 until 3295.7625 | 0 | 1 |
| 3295.7625 | Exit | 202 | Cashier | exits; total wait 12.0674, time in system 55.8493 | 0 | 0 |
| 3296.7244 | Move | 203 | Waiters | service done, routing to Cashier | 2 | 2 |
| 3296.7244 | Seize | 206 | Waiters | pulled from queue after waiting 13.7372, service until 3341.4015 | 1 | 3 |
| 3296.7244 | Seize | 203 | Cashier | server free, service 2.2611 until 3298.9856 | 0 | 1 |
| 3298.9856 | Exit | 203 | Cashier | exits; total wait 9.7145, time in system 46.1604 | 0 | 0 |
| 3305.3887 | Move | 204 | Waiters | service done, routing to Cashier | 1 | 2 |
| 3305.3887 | Seize | 207 | Waiters | pulled from queue after waiting 16.0325, service until 3349.9756 | 0 | 3 |
| 3305.3887 | Seize | 204 | Cashier | server free, service 2.6347 until 3308.0234 | 0 | 1 |
| 3308.0234 | Exit | 204 | Cashier | exits; total wait 19.5969, time in system 49.7732 | 0 | 0 |
| 3309.9566 | Arrival | 208 | Host | enters the system | 0 | 0 |
| 3309.9566 | Seize | 208 | Host | server free, service 1.0588 until 3311.0154 | 0 | 1 |
| 3311.0154 | Move | 208 | Host | service done, routing to Waiters | 0 | 0 |
| 3311.0154 | Queue | 208 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 3324.9646 | Move | 205 | Waiters | service done, routing to Cashier | 1 | 2 |
| 3324.9646 | Seize | 208 | Waiters | pulled from queue after waiting 13.9492, service until 3364.4139 | 0 | 3 |
| 3324.9646 | Seize | 205 | Cashier | server free, service 3.7092 until 3328.6738 | 0 | 1 |
| 3328.6738 | Exit | 205 | Cashier | exits; total wait 16.7914, time in system 52.2560 | 0 | 0 |
| 3336.1484 | Arrival | 209 | Host | enters the system | 0 | 0 |
| 3336.1484 | Seize | 209 | Host | server free, service 2.3050 until 3338.4534 | 0 | 1 |
| 3338.4534 | Move | 209 | Host | service done, routing to Waiters | 0 | 0 |
| 3338.4534 | Queue | 209 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 3341.4015 | Move | 206 | Waiters | service done, routing to Cashier | 1 | 2 |
| 3341.4015 | Seize | 209 | Waiters | pulled from queue after waiting 2.9481, service until 3374.4843 | 0 | 3 |
| 3341.4015 | Seize | 206 | Cashier | server free, service 2.4168 until 3343.8183 | 0 | 1 |
| 3342.3843 | Arrival | 210 | Host | enters the system | 0 | 0 |
| 3342.3843 | Seize | 210 | Host | server free, service 0.3767 until 3342.7611 | 0 | 1 |
| 3342.7611 | Move | 210 | Host | service done, routing to Waiters | 0 | 0 |
| 3342.7611 | Queue | 210 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 3343.8183 | Exit | 206 | Cashier | exits; total wait 13.7372, time in system 61.8553 | 0 | 0 |
| 3349.5877 | Arrival | 211 | Host | enters the system | 0 | 0 |
| 3349.5877 | Seize | 211 | Host | server free, service 3.8876 until 3353.4753 | 0 | 1 |
| 3349.9756 | Move | 207 | Waiters | service done, routing to Cashier | 1 | 2 |
| 3349.9756 | Seize | 210 | Waiters | pulled from queue after waiting 7.2145, service until 3383.3783 | 0 | 3 |
| 3349.9756 | Seize | 207 | Cashier | server free, service 3.1980 until 3353.1736 | 0 | 1 |
| 3353.1736 | Exit | 207 | Cashier | exits; total wait 16.0325, time in system 67.4839 | 0 | 0 |
| 3353.4753 | Move | 211 | Host | service done, routing to Waiters | 0 | 0 |
| 3353.4753 | Queue | 211 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 3364.4139 | Move | 208 | Waiters | service done, routing to Cashier | 1 | 2 |
| 3364.4139 | Seize | 211 | Waiters | pulled from queue after waiting 10.9386, service until 3406.9533 | 0 | 3 |
| 3364.4139 | Seize | 208 | Cashier | server free, service 3.1964 until 3367.6103 | 0 | 1 |
| 3367.6103 | Exit | 208 | Cashier | exits; total wait 13.9492, time in system 57.6537 | 0 | 0 |
| 3373.4845 | Arrival | 212 | Host | enters the system | 0 | 0 |
| 3373.4845 | Seize | 212 | Host | server free, service 1.1930 until 3374.6775 | 0 | 1 |
| 3374.4843 | Move | 209 | Waiters | service done, routing to Cashier | 0 | 2 |
| 3374.4843 | Seize | 209 | Cashier | server free, service 2.8144 until 3377.2987 | 0 | 1 |
| 3374.6775 | Move | 212 | Host | service done, routing to Waiters | 0 | 0 |
| 3374.6775 | Seize | 212 | Waiters | server free, service 25.2380 until 3399.9155 | 0 | 3 |
| 3377.2987 | Exit | 209 | Cashier | exits; total wait 2.9481, time in system 41.1503 | 0 | 0 |
| 3383.3783 | Move | 210 | Waiters | service done, routing to Cashier | 0 | 2 |
| 3383.3783 | Seize | 210 | Cashier | server free, service 2.3165 until 3385.6947 | 0 | 1 |
| 3385.6947 | Exit | 210 | Cashier | exits; total wait 7.2145, time in system 43.3104 | 0 | 0 |
| 3399.9155 | Move | 212 | Waiters | service done, routing to Cashier | 0 | 1 |
| 3399.9155 | Seize | 212 | Cashier | server free, service 2.6465 until 3402.5620 | 0 | 1 |
| 3402.5620 | Exit | 212 | Cashier | exits; total wait 0.0000, time in system 29.0775 | 0 | 0 |
| 3404.3260 | Arrival | 213 | Host | enters the system | 0 | 0 |
| 3404.3260 | Seize | 213 | Host | server free, service 0.2603 until 3404.5864 | 0 | 1 |
| 3404.5864 | Move | 213 | Host | service done, routing to Waiters | 0 | 0 |
| 3404.5864 | Seize | 213 | Waiters | server free, service 38.5425 until 3443.1289 | 0 | 2 |
| 3405.9989 | Arrival | 214 | Host | enters the system | 0 | 0 |
| 3405.9989 | Seize | 214 | Host | server free, service 1.8846 until 3407.8835 | 0 | 1 |
| 3406.9533 | Move | 211 | Waiters | service done, routing to Cashier | 0 | 1 |
| 3406.9533 | Seize | 211 | Cashier | server free, service 2.3287 until 3409.2819 | 0 | 1 |
| 3407.8835 | Move | 214 | Host | service done, routing to Waiters | 0 | 0 |
| 3407.8835 | Seize | 214 | Waiters | server free, service 40.8577 until 3448.7412 | 0 | 2 |
| 3409.2819 | Exit | 211 | Cashier | exits; total wait 10.9386, time in system 59.6943 | 0 | 0 |
| 3410.9713 | Arrival | 215 | Host | enters the system | 0 | 0 |
| 3410.9713 | Seize | 215 | Host | server free, service 0.5746 until 3411.5459 | 0 | 1 |
| 3411.5459 | Move | 215 | Host | service done, routing to Waiters | 0 | 0 |
| 3411.5459 | Seize | 215 | Waiters | server free, service 32.4938 until 3444.0397 | 0 | 3 |
| 3426.7533 | Arrival | 216 | Host | enters the system | 0 | 0 |
| 3426.7533 | Seize | 216 | Host | server free, service 1.7431 until 3428.4964 | 0 | 1 |
| 3428.4964 | Move | 216 | Host | service done, routing to Waiters | 0 | 0 |
| 3428.4964 | Queue | 216 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 3443.1289 | Move | 213 | Waiters | service done, routing to Cashier | 1 | 2 |
| 3443.1289 | Seize | 216 | Waiters | pulled from queue after waiting 14.6325, service until 3484.2919 | 0 | 3 |
| 3443.1289 | Seize | 213 | Cashier | server free, service 2.1126 until 3445.2416 | 0 | 1 |
| 3444.0397 | Move | 215 | Waiters | service done, routing to Cashier | 0 | 2 |
| 3444.0397 | Queue | 215 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 3445.2416 | Exit | 213 | Cashier | exits; total wait 0.0000, time in system 40.9155 | 1 | 0 |
| 3445.2416 | Seize | 215 | Cashier | pulled from queue after waiting 1.2019, service until 3447.1835 | 0 | 1 |
| 3447.1835 | Exit | 215 | Cashier | exits; total wait 1.2019, time in system 36.2123 | 0 | 0 |
| 3448.7412 | Move | 214 | Waiters | service done, routing to Cashier | 0 | 1 |
| 3448.7412 | Seize | 214 | Cashier | server free, service 3.3058 until 3452.0470 | 0 | 1 |
| 3452.0470 | Exit | 214 | Cashier | exits; total wait 0.0000, time in system 46.0480 | 0 | 0 |
| 3465.4500 | Arrival | 217 | Host | enters the system | 0 | 0 |
| 3465.4500 | Seize | 217 | Host | server free, service 2.4893 until 3467.9393 | 0 | 1 |
| 3467.9393 | Move | 217 | Host | service done, routing to Waiters | 0 | 0 |
| 3467.9393 | Seize | 217 | Waiters | server free, service 36.5965 until 3504.5359 | 0 | 2 |
| 3469.1935 | Arrival | 218 | Host | enters the system | 0 | 0 |
| 3469.1935 | Seize | 218 | Host | server free, service 2.4163 until 3471.6099 | 0 | 1 |
| 3471.6099 | Move | 218 | Host | service done, routing to Waiters | 0 | 0 |
| 3471.6099 | Seize | 218 | Waiters | server free, service 48.8077 until 3520.4175 | 0 | 3 |
| 3484.2919 | Move | 216 | Waiters | service done, routing to Cashier | 0 | 2 |
| 3484.2919 | Seize | 216 | Cashier | server free, service 3.7435 until 3488.0354 | 0 | 1 |
| 3488.0354 | Exit | 216 | Cashier | exits; total wait 14.6325, time in system 61.2821 | 0 | 0 |
| 3504.5359 | Move | 217 | Waiters | service done, routing to Cashier | 0 | 1 |
| 3504.5359 | Seize | 217 | Cashier | server free, service 2.8806 until 3507.4165 | 0 | 1 |
| 3507.4165 | Exit | 217 | Cashier | exits; total wait 0.0000, time in system 41.9665 | 0 | 0 |
| 3513.3004 | Arrival | 219 | Host | enters the system | 0 | 0 |
| 3513.3004 | Seize | 219 | Host | server free, service 1.6358 until 3514.9362 | 0 | 1 |
| 3514.9362 | Move | 219 | Host | service done, routing to Waiters | 0 | 0 |
| 3514.9362 | Seize | 219 | Waiters | server free, service 33.5631 until 3548.4992 | 0 | 2 |
| 3520.4175 | Move | 218 | Waiters | service done, routing to Cashier | 0 | 1 |
| 3520.4175 | Seize | 218 | Cashier | server free, service 3.5529 until 3523.9705 | 0 | 1 |
| 3523.9705 | Exit | 218 | Cashier | exits; total wait 0.0000, time in system 54.7769 | 0 | 0 |
| 3548.4992 | Move | 219 | Waiters | service done, routing to Cashier | 0 | 0 |
| 3548.4992 | Seize | 219 | Cashier | server free, service 3.1475 until 3551.6468 | 0 | 1 |
| 3551.6468 | Exit | 219 | Cashier | exits; total wait 0.0000, time in system 38.3464 | 0 | 0 |
| 3554.1773 | Arrival | 220 | Host | enters the system | 0 | 0 |
| 3554.1773 | Seize | 220 | Host | server free, service 2.1928 until 3556.3700 | 0 | 1 |
| 3556.3700 | Move | 220 | Host | service done, routing to Waiters | 0 | 0 |
| 3556.3700 | Seize | 220 | Waiters | server free, service 44.2117 until 3600.5818 | 0 | 1 |
| 3560.6704 | Arrival | 221 | Host | enters the system | 0 | 0 |
| 3560.6704 | Seize | 221 | Host | server free, service 0.5808 until 3561.2512 | 0 | 1 |
| 3561.2512 | Move | 221 | Host | service done, routing to Waiters | 0 | 0 |
| 3561.2512 | Seize | 221 | Waiters | server free, service 37.8120 until 3599.0632 | 0 | 2 |
| 3563.9029 | Arrival | 222 | Host | enters the system | 0 | 0 |
| 3563.9029 | Seize | 222 | Host | server free, service 1.8159 until 3565.7189 | 0 | 1 |
| 3565.7189 | Move | 222 | Host | service done, routing to Waiters | 0 | 0 |
| 3565.7189 | Seize | 222 | Waiters | server free, service 36.9163 until 3602.6352 | 0 | 3 |
| 3571.2366 | Arrival | 223 | Host | enters the system | 0 | 0 |
| 3571.2366 | Seize | 223 | Host | server free, service 4.0144 until 3575.2509 | 0 | 1 |
| 3575.2509 | Move | 223 | Host | service done, routing to Waiters | 0 | 0 |
| 3575.2509 | Queue | 223 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 3599.0632 | Move | 221 | Waiters | service done, routing to Cashier | 1 | 2 |
| 3599.0632 | Seize | 223 | Waiters | pulled from queue after waiting 23.8123, service until 3638.1882 | 0 | 3 |
| 3599.0632 | Seize | 221 | Cashier | server free, service 3.4874 until 3602.5506 | 0 | 1 |
| 3600.5818 | Move | 220 | Waiters | service done, routing to Cashier | 0 | 2 |
| 3600.5818 | Queue | 220 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 3602.5506 | Exit | 221 | Cashier | exits; total wait 0.0000, time in system 41.8802 | 1 | 0 |
| 3602.5506 | Seize | 220 | Cashier | pulled from queue after waiting 1.9688, service until 3604.6102 | 0 | 1 |
| 3602.6352 | Move | 222 | Waiters | service done, routing to Cashier | 0 | 1 |
| 3602.6352 | Queue | 222 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 3604.6102 | Exit | 220 | Cashier | exits; total wait 1.9688, time in system 50.4329 | 1 | 0 |
| 3604.6102 | Seize | 222 | Cashier | pulled from queue after waiting 1.9750, service until 3607.3709 | 0 | 1 |
| 3607.3709 | Exit | 222 | Cashier | exits; total wait 1.9750, time in system 43.4679 | 0 | 0 |
| 3608.6773 | Arrival | 224 | Host | enters the system | 0 | 0 |
| 3608.6773 | Seize | 224 | Host | server free, service 5.5021 until 3614.1794 | 0 | 1 |
| 3614.1794 | Move | 224 | Host | service done, routing to Waiters | 0 | 0 |
| 3614.1794 | Seize | 224 | Waiters | server free, service 40.7305 until 3654.9100 | 0 | 2 |
| 3615.5695 | Arrival | 225 | Host | enters the system | 0 | 0 |
| 3615.5695 | Seize | 225 | Host | server free, service 4.5410 until 3620.1106 | 0 | 1 |
| 3620.1106 | Move | 225 | Host | service done, routing to Waiters | 0 | 0 |
| 3620.1106 | Seize | 225 | Waiters | server free, service 36.6130 until 3656.7235 | 0 | 3 |
| 3634.7810 | Arrival | 226 | Host | enters the system | 0 | 0 |
| 3634.7810 | Seize | 226 | Host | server free, service 3.0577 until 3637.8387 | 0 | 1 |
| 3637.8387 | Move | 226 | Host | service done, routing to Waiters | 0 | 0 |
| 3637.8387 | Queue | 226 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 3638.1882 | Move | 223 | Waiters | service done, routing to Cashier | 1 | 2 |
| 3638.1882 | Seize | 226 | Waiters | pulled from queue after waiting 0.3495, service until 3664.1591 | 0 | 3 |
| 3638.1882 | Seize | 223 | Cashier | server free, service 3.4833 until 3641.6716 | 0 | 1 |
| 3641.6716 | Exit | 223 | Cashier | exits; total wait 23.8123, time in system 70.4350 | 0 | 0 |
| 3649.3749 | Arrival | 227 | Host | enters the system | 0 | 0 |
| 3649.3749 | Seize | 227 | Host | server free, service 0.6172 until 3649.9921 | 0 | 1 |
| 3649.9921 | Move | 227 | Host | service done, routing to Waiters | 0 | 0 |
| 3649.9921 | Queue | 227 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 3654.9100 | Move | 224 | Waiters | service done, routing to Cashier | 1 | 2 |
| 3654.9100 | Seize | 227 | Waiters | pulled from queue after waiting 4.9178, service until 3690.7721 | 0 | 3 |
| 3654.9100 | Seize | 224 | Cashier | server free, service 1.2433 until 3656.1533 | 0 | 1 |
| 3656.1533 | Exit | 224 | Cashier | exits; total wait 0.0000, time in system 47.4759 | 0 | 0 |
| 3656.3429 | Arrival | 228 | Host | enters the system | 0 | 0 |
| 3656.3429 | Seize | 228 | Host | server free, service 1.3513 until 3657.6942 | 0 | 1 |
| 3656.7235 | Move | 225 | Waiters | service done, routing to Cashier | 0 | 2 |
| 3656.7235 | Seize | 225 | Cashier | server free, service 2.6603 until 3659.3839 | 0 | 1 |
| 3657.6942 | Move | 228 | Host | service done, routing to Waiters | 0 | 0 |
| 3657.6942 | Seize | 228 | Waiters | server free, service 30.1728 until 3687.8670 | 0 | 3 |
| 3659.3839 | Exit | 225 | Cashier | exits; total wait 0.0000, time in system 43.8143 | 0 | 0 |
| 3664.1591 | Move | 226 | Waiters | service done, routing to Cashier | 0 | 2 |
| 3664.1591 | Seize | 226 | Cashier | server free, service 2.0114 until 3666.1706 | 0 | 1 |
| 3666.1706 | Exit | 226 | Cashier | exits; total wait 0.3495, time in system 31.3896 | 0 | 0 |
| 3687.8670 | Move | 228 | Waiters | service done, routing to Cashier | 0 | 1 |
| 3687.8670 | Seize | 228 | Cashier | server free, service 2.9254 until 3690.7924 | 0 | 1 |
| 3690.7721 | Move | 227 | Waiters | service done, routing to Cashier | 0 | 0 |
| 3690.7721 | Queue | 227 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 3690.7924 | Exit | 228 | Cashier | exits; total wait 0.0000, time in system 34.4495 | 1 | 0 |
| 3690.7924 | Seize | 227 | Cashier | pulled from queue after waiting 0.0203, service until 3694.4760 | 0 | 1 |
| 3694.4760 | Exit | 227 | Cashier | exits; total wait 4.9381, time in system 45.1011 | 0 | 0 |
| 3718.4332 | Arrival | 229 | Host | enters the system | 0 | 0 |
| 3718.4332 | Seize | 229 | Host | server free, service 9.4285 until 3727.8617 | 0 | 1 |
| 3727.8617 | Move | 229 | Host | service done, routing to Waiters | 0 | 0 |
| 3727.8617 | Seize | 229 | Waiters | server free, service 31.3584 until 3759.2201 | 0 | 1 |
| 3759.2201 | Move | 229 | Waiters | service done, routing to Cashier | 0 | 0 |
| 3759.2201 | Seize | 229 | Cashier | server free, service 2.3338 until 3761.5540 | 0 | 1 |
| 3761.5540 | Exit | 229 | Cashier | exits; total wait 0.0000, time in system 43.1208 | 0 | 0 |
| 3764.7339 | Arrival | 230 | Host | enters the system | 0 | 0 |
| 3764.7339 | Seize | 230 | Host | server free, service 0.5478 until 3765.2817 | 0 | 1 |
| 3765.2817 | Move | 230 | Host | service done, routing to Waiters | 0 | 0 |
| 3765.2817 | Seize | 230 | Waiters | server free, service 27.3177 until 3792.5994 | 0 | 1 |
| 3786.5740 | Arrival | 231 | Host | enters the system | 0 | 0 |
| 3786.5740 | Seize | 231 | Host | server free, service 9.2668 until 3795.8408 | 0 | 1 |
| 3792.5994 | Move | 230 | Waiters | service done, routing to Cashier | 0 | 0 |
| 3792.5994 | Seize | 230 | Cashier | server free, service 3.3529 until 3795.9523 | 0 | 1 |
| 3795.8408 | Move | 231 | Host | service done, routing to Waiters | 0 | 0 |
| 3795.8408 | Seize | 231 | Waiters | server free, service 37.7639 until 3833.6047 | 0 | 1 |
| 3795.9523 | Exit | 230 | Cashier | exits; total wait 0.0000, time in system 31.2184 | 0 | 0 |
| 3798.3348 | Arrival | 232 | Host | enters the system | 0 | 0 |
| 3798.3348 | Seize | 232 | Host | server free, service 5.0137 until 3803.3485 | 0 | 1 |
| 3803.3485 | Move | 232 | Host | service done, routing to Waiters | 0 | 0 |
| 3803.3485 | Seize | 232 | Waiters | server free, service 40.9903 until 3844.3388 | 0 | 2 |
| 3806.2387 | Arrival | 233 | Host | enters the system | 0 | 0 |
| 3806.2387 | Seize | 233 | Host | server free, service 3.4964 until 3809.7352 | 0 | 1 |
| 3809.7352 | Move | 233 | Host | service done, routing to Waiters | 0 | 0 |
| 3809.7352 | Seize | 233 | Waiters | server free, service 35.3411 until 3845.0763 | 0 | 3 |
| 3833.6047 | Move | 231 | Waiters | service done, routing to Cashier | 0 | 2 |
| 3833.6047 | Seize | 231 | Cashier | server free, service 3.8665 until 3837.4712 | 0 | 1 |
| 3837.4712 | Exit | 231 | Cashier | exits; total wait 0.0000, time in system 50.8972 | 0 | 0 |
| 3841.5469 | Arrival | 234 | Host | enters the system | 0 | 0 |
| 3841.5469 | Seize | 234 | Host | server free, service 2.0256 until 3843.5725 | 0 | 1 |
| 3843.5725 | Move | 234 | Host | service done, routing to Waiters | 0 | 0 |
| 3843.5725 | Seize | 234 | Waiters | server free, service 30.6317 until 3874.2042 | 0 | 3 |
| 3844.3388 | Move | 232 | Waiters | service done, routing to Cashier | 0 | 2 |
| 3844.3388 | Seize | 232 | Cashier | server free, service 2.0768 until 3846.4156 | 0 | 1 |
| 3844.6265 | Arrival | 235 | Host | enters the system | 0 | 0 |
| 3844.6265 | Seize | 235 | Host | server free, service 2.9635 until 3847.5900 | 0 | 1 |
| 3845.0763 | Move | 233 | Waiters | service done, routing to Cashier | 0 | 1 |
| 3845.0763 | Queue | 233 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 3846.4156 | Exit | 232 | Cashier | exits; total wait 0.0000, time in system 48.0808 | 1 | 0 |
| 3846.4156 | Seize | 233 | Cashier | pulled from queue after waiting 1.3394, service until 3850.2036 | 0 | 1 |
| 3847.5900 | Move | 235 | Host | service done, routing to Waiters | 0 | 0 |
| 3847.5900 | Seize | 235 | Waiters | server free, service 23.2922 until 3870.8822 | 0 | 2 |
| 3850.2036 | Exit | 233 | Cashier | exits; total wait 1.3394, time in system 43.9649 | 0 | 0 |
| 3852.9420 | Arrival | 236 | Host | enters the system | 0 | 0 |
| 3852.9420 | Seize | 236 | Host | server free, service 2.0446 until 3854.9865 | 0 | 1 |
| 3854.9865 | Move | 236 | Host | service done, routing to Waiters | 0 | 0 |
| 3854.9865 | Seize | 236 | Waiters | server free, service 34.5912 until 3889.5777 | 0 | 3 |
| 3858.2085 | Arrival | 237 | Host | enters the system | 0 | 0 |
| 3858.2085 | Seize | 237 | Host | server free, service 0.2146 until 3858.4231 | 0 | 1 |
| 3858.4231 | Move | 237 | Host | service done, routing to Waiters | 0 | 0 |
| 3858.4231 | Queue | 237 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 3868.1547 | Arrival | 238 | Host | enters the system | 0 | 0 |
| 3868.1547 | Seize | 238 | Host | server free, service 9.2996 until 3877.4544 | 0 | 1 |
| 3870.8822 | Move | 235 | Waiters | service done, routing to Cashier | 1 | 2 |
| 3870.8822 | Seize | 237 | Waiters | pulled from queue after waiting 12.4592, service until 3900.9332 | 0 | 3 |
| 3870.8822 | Seize | 235 | Cashier | server free, service 1.9114 until 3872.7936 | 0 | 1 |
| 3872.7936 | Exit | 235 | Cashier | exits; total wait 0.0000, time in system 28.1670 | 0 | 0 |
| 3874.0279 | Arrival | 239 | Host | enters the system | 0 | 1 |
| 3874.0279 | Queue | 239 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 3874.2042 | Move | 234 | Waiters | service done, routing to Cashier | 0 | 2 |
| 3874.2042 | Seize | 234 | Cashier | server free, service 2.0008 until 3876.2050 | 0 | 1 |
| 3876.2050 | Exit | 234 | Cashier | exits; total wait 0.0000, time in system 34.6580 | 0 | 0 |
| 3876.9314 | Arrival | 240 | Host | enters the system | 1 | 1 |
| 3876.9314 | Queue | 240 | Host | all 1 busy, queued at position 2 | 2 | 1 |
| 3877.4544 | Move | 238 | Host | service done, routing to Waiters | 2 | 0 |
| 3877.4544 | Seize | 239 | Host | pulled from queue after waiting 3.4264, service until 3881.4198 | 1 | 1 |
| 3877.4544 | Seize | 238 | Waiters | server free, service 38.8711 until 3916.3255 | 0 | 3 |
| 3881.4198 | Move | 239 | Host | service done, routing to Waiters | 1 | 0 |
| 3881.4198 | Seize | 240 | Host | pulled from queue after waiting 4.4884, service until 3882.7248 | 0 | 1 |
| 3881.4198 | Queue | 239 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 3882.7248 | Move | 240 | Host | service done, routing to Waiters | 0 | 0 |
| 3882.7248 | Queue | 240 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 3883.8806 | Arrival | 241 | Host | enters the system | 0 | 0 |
| 3883.8806 | Seize | 241 | Host | server free, service 0.9874 until 3884.8680 | 0 | 1 |
| 3884.8680 | Move | 241 | Host | service done, routing to Waiters | 0 | 0 |
| 3884.8680 | Queue | 241 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 3888.2570 | Arrival | 242 | Host | enters the system | 0 | 0 |
| 3888.2570 | Seize | 242 | Host | server free, service 0.2545 until 3888.5114 | 0 | 1 |
| 3888.5114 | Move | 242 | Host | service done, routing to Waiters | 0 | 0 |
| 3888.5114 | Queue | 242 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 3889.0005 | Arrival | 243 | Host | enters the system | 0 | 0 |
| 3889.0005 | Seize | 243 | Host | server free, service 0.1866 until 3889.1871 | 0 | 1 |
| 3889.1871 | Move | 243 | Host | service done, routing to Waiters | 0 | 0 |
| 3889.1871 | Queue | 243 | Waiters | all 3 busy, queued at position 5 | 5 | 3 |
| 3889.5777 | Move | 236 | Waiters | service done, routing to Cashier | 5 | 2 |
| 3889.5777 | Seize | 239 | Waiters | pulled from queue after waiting 8.1579, service until 3939.7168 | 4 | 3 |
| 3889.5777 | Seize | 236 | Cashier | server free, service 2.7736 until 3892.3513 | 0 | 1 |
| 3892.3513 | Exit | 236 | Cashier | exits; total wait 0.0000, time in system 39.4094 | 0 | 0 |
| 3900.9332 | Move | 237 | Waiters | service done, routing to Cashier | 4 | 2 |
| 3900.9332 | Seize | 240 | Waiters | pulled from queue after waiting 18.2084, service until 3950.5649 | 3 | 3 |
| 3900.9332 | Seize | 237 | Cashier | server free, service 2.3938 until 3903.3271 | 0 | 1 |
| 3903.3271 | Exit | 237 | Cashier | exits; total wait 12.4592, time in system 45.1186 | 0 | 0 |
| 3909.8276 | Arrival | 244 | Host | enters the system | 0 | 0 |
| 3909.8276 | Seize | 244 | Host | server free, service 2.8757 until 3912.7033 | 0 | 1 |
| 3912.7033 | Move | 244 | Host | service done, routing to Waiters | 0 | 0 |
| 3912.7033 | Queue | 244 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 3916.3255 | Move | 238 | Waiters | service done, routing to Cashier | 4 | 2 |
| 3916.3255 | Seize | 241 | Waiters | pulled from queue after waiting 31.4575, service until 3953.6540 | 3 | 3 |
| 3916.3255 | Seize | 238 | Cashier | server free, service 1.7942 until 3918.1197 | 0 | 1 |
| 3918.1197 | Exit | 238 | Cashier | exits; total wait 0.0000, time in system 49.9650 | 0 | 0 |
| 3928.5770 | Arrival | 245 | Host | enters the system | 0 | 0 |
| 3928.5770 | Seize | 245 | Host | server free, service 0.6449 until 3929.2219 | 0 | 1 |
| 3929.2219 | Move | 245 | Host | service done, routing to Waiters | 0 | 0 |
| 3929.2219 | Queue | 245 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 3939.7168 | Move | 239 | Waiters | service done, routing to Cashier | 4 | 2 |
| 3939.7168 | Seize | 242 | Waiters | pulled from queue after waiting 51.2053, service until 3984.6433 | 3 | 3 |
| 3939.7168 | Seize | 239 | Cashier | server free, service 1.0888 until 3940.8056 | 0 | 1 |
| 3940.8056 | Exit | 239 | Cashier | exits; total wait 11.5843, time in system 66.7776 | 0 | 0 |
| 3943.8048 | Arrival | 246 | Host | enters the system | 0 | 0 |
| 3943.8048 | Seize | 246 | Host | server free, service 1.2688 until 3945.0736 | 0 | 1 |
| 3945.0736 | Move | 246 | Host | service done, routing to Waiters | 0 | 0 |
| 3945.0736 | Queue | 246 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 3950.5649 | Move | 240 | Waiters | service done, routing to Cashier | 4 | 2 |
| 3950.5649 | Seize | 243 | Waiters | pulled from queue after waiting 61.3778, service until 3994.0223 | 3 | 3 |
| 3950.5649 | Seize | 240 | Cashier | server free, service 3.3961 until 3953.9610 | 0 | 1 |
| 3953.6540 | Move | 241 | Waiters | service done, routing to Cashier | 3 | 2 |
| 3953.6540 | Seize | 244 | Waiters | pulled from queue after waiting 40.9507, service until 4003.6472 | 2 | 3 |
| 3953.6540 | Queue | 241 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 3953.9610 | Exit | 240 | Cashier | exits; total wait 22.6968, time in system 77.0296 | 1 | 0 |
| 3953.9610 | Seize | 241 | Cashier | pulled from queue after waiting 0.3071, service until 3955.5502 | 0 | 1 |
| 3955.5352 | Arrival | 247 | Host | enters the system | 0 | 0 |
| 3955.5352 | Seize | 247 | Host | server free, service 3.0472 until 3958.5824 | 0 | 1 |
| 3955.5502 | Exit | 241 | Cashier | exits; total wait 31.7646, time in system 71.6696 | 0 | 0 |
| 3958.5824 | Move | 247 | Host | service done, routing to Waiters | 0 | 0 |
| 3958.5824 | Queue | 247 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 3984.6433 | Move | 242 | Waiters | service done, routing to Cashier | 3 | 2 |
| 3984.6433 | Seize | 245 | Waiters | pulled from queue after waiting 55.4214, service until 4024.6539 | 2 | 3 |
| 3984.6433 | Seize | 242 | Cashier | server free, service 3.3419 until 3987.9852 | 0 | 1 |
| 3985.6276 | Arrival | 248 | Host | enters the system | 0 | 0 |
| 3985.6276 | Seize | 248 | Host | server free, service 5.1325 until 3990.7601 | 0 | 1 |
| 3987.9852 | Exit | 242 | Cashier | exits; total wait 51.2053, time in system 99.7282 | 0 | 0 |
| 3990.1256 | Arrival | 249 | Host | enters the system | 0 | 1 |
| 3990.1256 | Queue | 249 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 3990.7601 | Move | 248 | Host | service done, routing to Waiters | 1 | 0 |
| 3990.7601 | Seize | 249 | Host | pulled from queue after waiting 0.6345, service until 3991.8153 | 0 | 1 |
| 3990.7601 | Queue | 248 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 3991.8153 | Move | 249 | Host | service done, routing to Waiters | 0 | 0 |
| 3991.8153 | Queue | 249 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 3994.0223 | Move | 243 | Waiters | service done, routing to Cashier | 4 | 2 |
| 3994.0223 | Seize | 246 | Waiters | pulled from queue after waiting 48.9487, service until 4028.0203 | 3 | 3 |
| 3994.0223 | Seize | 243 | Cashier | server free, service 2.6447 until 3996.6670 | 0 | 1 |
| 3996.6670 | Exit | 243 | Cashier | exits; total wait 61.3778, time in system 107.6665 | 0 | 0 |
| 4003.6472 | Move | 244 | Waiters | service done, routing to Cashier | 3 | 2 |
| 4003.6472 | Seize | 247 | Waiters | pulled from queue after waiting 45.0648, service until 4030.5768 | 2 | 3 |
| 4003.6472 | Seize | 244 | Cashier | server free, service 3.1051 until 4006.7523 | 0 | 1 |
| 4006.7523 | Exit | 244 | Cashier | exits; total wait 40.9507, time in system 96.9247 | 0 | 0 |
| 4024.6539 | Move | 245 | Waiters | service done, routing to Cashier | 2 | 2 |
| 4024.6539 | Seize | 248 | Waiters | pulled from queue after waiting 33.8938, service until 4058.6964 | 1 | 3 |
| 4024.6539 | Seize | 245 | Cashier | server free, service 1.9534 until 4026.6073 | 0 | 1 |
| 4026.6073 | Exit | 245 | Cashier | exits; total wait 55.4214, time in system 98.0302 | 0 | 0 |
| 4028.0203 | Move | 246 | Waiters | service done, routing to Cashier | 1 | 2 |
| 4028.0203 | Seize | 249 | Waiters | pulled from queue after waiting 36.2050, service until 4053.8001 | 0 | 3 |
| 4028.0203 | Seize | 246 | Cashier | server free, service 1.5319 until 4029.5521 | 0 | 1 |
| 4029.5521 | Exit | 246 | Cashier | exits; total wait 48.9487, time in system 85.7473 | 0 | 0 |
| 4030.5768 | Move | 247 | Waiters | service done, routing to Cashier | 0 | 2 |
| 4030.5768 | Seize | 247 | Cashier | server free, service 1.3547 until 4031.9315 | 0 | 1 |
| 4031.9315 | Exit | 247 | Cashier | exits; total wait 45.0648, time in system 76.3963 | 0 | 0 |
| 4045.6969 | Arrival | 250 | Host | enters the system | 0 | 0 |
| 4045.6969 | Seize | 250 | Host | server free, service 2.5290 until 4048.2259 | 0 | 1 |
| 4048.2259 | Move | 250 | Host | service done, routing to Waiters | 0 | 0 |
| 4048.2259 | Seize | 250 | Waiters | server free, service 54.3293 until 4102.5552 | 0 | 3 |
| 4051.3074 | Arrival | 251 | Host | enters the system | 0 | 0 |
| 4051.3074 | Seize | 251 | Host | server free, service 0.0402 until 4051.3476 | 0 | 1 |
| 4051.3476 | Move | 251 | Host | service done, routing to Waiters | 0 | 0 |
| 4051.3476 | Queue | 251 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 4053.8001 | Move | 249 | Waiters | service done, routing to Cashier | 1 | 2 |
| 4053.8001 | Seize | 251 | Waiters | pulled from queue after waiting 2.4526, service until 4086.9260 | 0 | 3 |
| 4053.8001 | Seize | 249 | Cashier | server free, service 1.3587 until 4055.1588 | 0 | 1 |
| 4055.1588 | Exit | 249 | Cashier | exits; total wait 36.8395, time in system 65.0333 | 0 | 0 |
| 4058.6964 | Move | 248 | Waiters | service done, routing to Cashier | 0 | 2 |
| 4058.6964 | Seize | 248 | Cashier | server free, service 3.9815 until 4062.6779 | 0 | 1 |
| 4062.6779 | Exit | 248 | Cashier | exits; total wait 33.8938, time in system 77.0504 | 0 | 0 |
| 4069.8947 | Arrival | 252 | Host | enters the system | 0 | 0 |
| 4069.8947 | Seize | 252 | Host | server free, service 1.9842 until 4071.8789 | 0 | 1 |
| 4071.8789 | Move | 252 | Host | service done, routing to Waiters | 0 | 0 |
| 4071.8789 | Seize | 252 | Waiters | server free, service 28.5029 until 4100.3818 | 0 | 3 |
| 4086.9260 | Move | 251 | Waiters | service done, routing to Cashier | 0 | 2 |
| 4086.9260 | Seize | 251 | Cashier | server free, service 3.9227 until 4090.8487 | 0 | 1 |
| 4090.8487 | Exit | 251 | Cashier | exits; total wait 2.4526, time in system 39.5414 | 0 | 0 |
| 4095.6518 | Arrival | 253 | Host | enters the system | 0 | 0 |
| 4095.6518 | Seize | 253 | Host | server free, service 2.3555 until 4098.0073 | 0 | 1 |
| 4097.6733 | Arrival | 254 | Host | enters the system | 0 | 1 |
| 4097.6733 | Queue | 254 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 4098.0073 | Move | 253 | Host | service done, routing to Waiters | 1 | 0 |
| 4098.0073 | Seize | 254 | Host | pulled from queue after waiting 0.3340, service until 4098.5055 | 0 | 1 |
| 4098.0073 | Seize | 253 | Waiters | server free, service 45.0125 until 4143.0198 | 0 | 3 |
| 4098.5055 | Move | 254 | Host | service done, routing to Waiters | 0 | 0 |
| 4098.5055 | Queue | 254 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 4100.3818 | Move | 252 | Waiters | service done, routing to Cashier | 1 | 2 |
| 4100.3818 | Seize | 254 | Waiters | pulled from queue after waiting 1.8763, service until 4129.0991 | 0 | 3 |
| 4100.3818 | Seize | 252 | Cashier | server free, service 3.3948 until 4103.7766 | 0 | 1 |
| 4102.5552 | Move | 250 | Waiters | service done, routing to Cashier | 0 | 2 |
| 4102.5552 | Queue | 250 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 4103.7766 | Exit | 252 | Cashier | exits; total wait 0.0000, time in system 33.8819 | 1 | 0 |
| 4103.7766 | Seize | 250 | Cashier | pulled from queue after waiting 1.2214, service until 4106.0357 | 0 | 1 |
| 4106.0357 | Exit | 250 | Cashier | exits; total wait 1.2214, time in system 60.3388 | 0 | 0 |
| 4110.0041 | Arrival | 255 | Host | enters the system | 0 | 0 |
| 4110.0041 | Seize | 255 | Host | server free, service 0.4791 until 4110.4832 | 0 | 1 |
| 4110.4832 | Move | 255 | Host | service done, routing to Waiters | 0 | 0 |
| 4110.4832 | Seize | 255 | Waiters | server free, service 46.1984 until 4156.6816 | 0 | 3 |
| 4110.9898 | Arrival | 256 | Host | enters the system | 0 | 0 |
| 4110.9898 | Seize | 256 | Host | server free, service 0.1138 until 4111.1035 | 0 | 1 |
| 4111.1035 | Move | 256 | Host | service done, routing to Waiters | 0 | 0 |
| 4111.1035 | Queue | 256 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 4120.4998 | Arrival | 257 | Host | enters the system | 0 | 0 |
| 4120.4998 | Seize | 257 | Host | server free, service 0.2535 until 4120.7534 | 0 | 1 |
| 4120.7534 | Move | 257 | Host | service done, routing to Waiters | 0 | 0 |
| 4120.7534 | Queue | 257 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 4129.0991 | Move | 254 | Waiters | service done, routing to Cashier | 2 | 2 |
| 4129.0991 | Seize | 256 | Waiters | pulled from queue after waiting 17.9955, service until 4159.3795 | 1 | 3 |
| 4129.0991 | Seize | 254 | Cashier | server free, service 2.6935 until 4131.7926 | 0 | 1 |
| 4131.7926 | Exit | 254 | Cashier | exits; total wait 2.2103, time in system 34.1193 | 0 | 0 |
| 4135.5970 | Arrival | 258 | Host | enters the system | 0 | 0 |
| 4135.5970 | Seize | 258 | Host | server free, service 0.3027 until 4135.8997 | 0 | 1 |
| 4135.8997 | Move | 258 | Host | service done, routing to Waiters | 0 | 0 |
| 4135.8997 | Queue | 258 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 4143.0198 | Move | 253 | Waiters | service done, routing to Cashier | 2 | 2 |
| 4143.0198 | Seize | 257 | Waiters | pulled from queue after waiting 22.2664, service until 4189.6185 | 1 | 3 |
| 4143.0198 | Seize | 253 | Cashier | server free, service 3.0522 until 4146.0720 | 0 | 1 |
| 4146.0720 | Exit | 253 | Cashier | exits; total wait 0.0000, time in system 50.4203 | 0 | 0 |
| 4156.6816 | Move | 255 | Waiters | service done, routing to Cashier | 1 | 2 |
| 4156.6816 | Seize | 258 | Waiters | pulled from queue after waiting 20.7819, service until 4211.7634 | 0 | 3 |
| 4156.6816 | Seize | 255 | Cashier | server free, service 2.6898 until 4159.3715 | 0 | 1 |
| 4159.3715 | Exit | 255 | Cashier | exits; total wait 0.0000, time in system 49.3674 | 0 | 0 |
| 4159.3795 | Move | 256 | Waiters | service done, routing to Cashier | 0 | 2 |
| 4159.3795 | Seize | 256 | Cashier | server free, service 1.8228 until 4161.2024 | 0 | 1 |
| 4161.2024 | Exit | 256 | Cashier | exits; total wait 17.9955, time in system 50.2126 | 0 | 0 |
| 4166.0133 | Arrival | 259 | Host | enters the system | 0 | 0 |
| 4166.0133 | Seize | 259 | Host | server free, service 0.8038 until 4166.8170 | 0 | 1 |
| 4166.8170 | Move | 259 | Host | service done, routing to Waiters | 0 | 0 |
| 4166.8170 | Seize | 259 | Waiters | server free, service 42.6244 until 4209.4415 | 0 | 3 |
| 4179.8320 | Arrival | 260 | Host | enters the system | 0 | 0 |
| 4179.8320 | Seize | 260 | Host | server free, service 0.5165 until 4180.3486 | 0 | 1 |
| 4180.3486 | Move | 260 | Host | service done, routing to Waiters | 0 | 0 |
| 4180.3486 | Queue | 260 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 4189.6185 | Move | 257 | Waiters | service done, routing to Cashier | 1 | 2 |
| 4189.6185 | Seize | 260 | Waiters | pulled from queue after waiting 9.2700, service until 4240.3912 | 0 | 3 |
| 4189.6185 | Seize | 257 | Cashier | server free, service 2.4140 until 4192.0325 | 0 | 1 |
| 4192.0325 | Exit | 257 | Cashier | exits; total wait 22.2664, time in system 71.5327 | 0 | 0 |
| 4209.4415 | Move | 259 | Waiters | service done, routing to Cashier | 0 | 2 |
| 4209.4415 | Seize | 259 | Cashier | server free, service 1.9172 until 4211.3587 | 0 | 1 |
| 4211.3587 | Exit | 259 | Cashier | exits; total wait 0.0000, time in system 45.3454 | 0 | 0 |
| 4211.7634 | Move | 258 | Waiters | service done, routing to Cashier | 0 | 1 |
| 4211.7634 | Seize | 258 | Cashier | server free, service 3.0483 until 4214.8117 | 0 | 1 |
| 4214.8117 | Exit | 258 | Cashier | exits; total wait 20.7819, time in system 79.2147 | 0 | 0 |
| 4215.1082 | Arrival | 261 | Host | enters the system | 0 | 0 |
| 4215.1082 | Seize | 261 | Host | server free, service 0.3637 until 4215.4719 | 0 | 1 |
| 4215.4719 | Move | 261 | Host | service done, routing to Waiters | 0 | 0 |
| 4215.4719 | Seize | 261 | Waiters | server free, service 24.9187 until 4240.3906 | 0 | 2 |
| 4219.3391 | Arrival | 262 | Host | enters the system | 0 | 0 |
| 4219.3391 | Seize | 262 | Host | server free, service 1.6022 until 4220.9413 | 0 | 1 |
| 4220.9413 | Move | 262 | Host | service done, routing to Waiters | 0 | 0 |
| 4220.9413 | Seize | 262 | Waiters | server free, service 37.2794 until 4258.2207 | 0 | 3 |
| 4224.8201 | Arrival | 263 | Host | enters the system | 0 | 0 |
| 4224.8201 | Seize | 263 | Host | server free, service 6.2914 until 4231.1115 | 0 | 1 |
| 4231.1115 | Move | 263 | Host | service done, routing to Waiters | 0 | 0 |
| 4231.1115 | Queue | 263 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 4240.3906 | Move | 261 | Waiters | service done, routing to Cashier | 1 | 2 |
| 4240.3906 | Seize | 263 | Waiters | pulled from queue after waiting 9.2791, service until 4276.0055 | 0 | 3 |
| 4240.3906 | Seize | 261 | Cashier | server free, service 3.0821 until 4243.4727 | 0 | 1 |
| 4240.3912 | Move | 260 | Waiters | service done, routing to Cashier | 0 | 2 |
| 4240.3912 | Queue | 260 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 4241.3579 | Arrival | 264 | Host | enters the system | 0 | 0 |
| 4241.3579 | Seize | 264 | Host | server free, service 1.4977 until 4242.8557 | 0 | 1 |
| 4242.8557 | Move | 264 | Host | service done, routing to Waiters | 0 | 0 |
| 4242.8557 | Seize | 264 | Waiters | server free, service 27.8320 until 4270.6877 | 0 | 3 |
| 4243.4727 | Exit | 261 | Cashier | exits; total wait 0.0000, time in system 28.3645 | 1 | 0 |
| 4243.4727 | Seize | 260 | Cashier | pulled from queue after waiting 3.0816, service until 4245.7730 | 0 | 1 |
| 4245.6077 | Arrival | 265 | Host | enters the system | 0 | 0 |
| 4245.6077 | Seize | 265 | Host | server free, service 1.4905 until 4247.0982 | 0 | 1 |
| 4245.7730 | Exit | 260 | Cashier | exits; total wait 12.3515, time in system 65.9410 | 0 | 0 |
| 4247.0982 | Move | 265 | Host | service done, routing to Waiters | 0 | 0 |
| 4247.0982 | Queue | 265 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 4254.0448 | Arrival | 266 | Host | enters the system | 0 | 0 |
| 4254.0448 | Seize | 266 | Host | server free, service 1.5737 until 4255.6186 | 0 | 1 |
| 4255.6186 | Move | 266 | Host | service done, routing to Waiters | 0 | 0 |
| 4255.6186 | Queue | 266 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 4258.2207 | Move | 262 | Waiters | service done, routing to Cashier | 2 | 2 |
| 4258.2207 | Seize | 265 | Waiters | pulled from queue after waiting 11.1225, service until 4292.7068 | 1 | 3 |
| 4258.2207 | Seize | 262 | Cashier | server free, service 3.2486 until 4261.4693 | 0 | 1 |
| 4261.4693 | Exit | 262 | Cashier | exits; total wait 0.0000, time in system 42.1301 | 0 | 0 |
| 4270.6877 | Move | 264 | Waiters | service done, routing to Cashier | 1 | 2 |
| 4270.6877 | Seize | 266 | Waiters | pulled from queue after waiting 15.0691, service until 4311.3252 | 0 | 3 |
| 4270.6877 | Seize | 264 | Cashier | server free, service 3.5326 until 4274.2202 | 0 | 1 |
| 4274.2202 | Exit | 264 | Cashier | exits; total wait 0.0000, time in system 32.8623 | 0 | 0 |
| 4276.0055 | Move | 263 | Waiters | service done, routing to Cashier | 0 | 2 |
| 4276.0055 | Seize | 263 | Cashier | server free, service 3.8921 until 4279.8976 | 0 | 1 |
| 4279.8976 | Exit | 263 | Cashier | exits; total wait 9.2791, time in system 55.0774 | 0 | 0 |
| 4292.7068 | Move | 265 | Waiters | service done, routing to Cashier | 0 | 1 |
| 4292.7068 | Seize | 265 | Cashier | server free, service 1.0736 until 4293.7804 | 0 | 1 |
| 4293.7804 | Exit | 265 | Cashier | exits; total wait 11.1225, time in system 48.1727 | 0 | 0 |
| 4295.6721 | Arrival | 267 | Host | enters the system | 0 | 0 |
| 4295.6721 | Seize | 267 | Host | server free, service 2.5938 until 4298.2659 | 0 | 1 |
| 4297.0184 | Arrival | 268 | Host | enters the system | 0 | 1 |
| 4297.0184 | Queue | 268 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 4298.2659 | Move | 267 | Host | service done, routing to Waiters | 1 | 0 |
| 4298.2659 | Seize | 268 | Host | pulled from queue after waiting 1.2475, service until 4305.4278 | 0 | 1 |
| 4298.2659 | Seize | 267 | Waiters | server free, service 32.7710 until 4331.0369 | 0 | 2 |
| 4305.4278 | Move | 268 | Host | service done, routing to Waiters | 0 | 0 |
| 4305.4278 | Seize | 268 | Waiters | server free, service 44.5911 until 4350.0189 | 0 | 3 |
| 4311.3252 | Move | 266 | Waiters | service done, routing to Cashier | 0 | 2 |
| 4311.3252 | Seize | 266 | Cashier | server free, service 2.1684 until 4313.4936 | 0 | 1 |
| 4313.4936 | Exit | 266 | Cashier | exits; total wait 15.0691, time in system 59.4488 | 0 | 0 |
| 4320.9516 | Arrival | 269 | Host | enters the system | 0 | 0 |
| 4320.9516 | Seize | 269 | Host | server free, service 1.9645 until 4322.9161 | 0 | 1 |
| 4322.7966 | Arrival | 270 | Host | enters the system | 0 | 1 |
| 4322.7966 | Queue | 270 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 4322.9161 | Move | 269 | Host | service done, routing to Waiters | 1 | 0 |
| 4322.9161 | Seize | 270 | Host | pulled from queue after waiting 0.1195, service until 4327.7169 | 0 | 1 |
| 4322.9161 | Seize | 269 | Waiters | server free, service 46.7076 until 4369.6237 | 0 | 3 |
| 4327.4064 | Arrival | 271 | Host | enters the system | 0 | 1 |
| 4327.4064 | Queue | 271 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 4327.7169 | Move | 270 | Host | service done, routing to Waiters | 1 | 0 |
| 4327.7169 | Seize | 271 | Host | pulled from queue after waiting 0.3105, service until 4332.5834 | 0 | 1 |
| 4327.7169 | Queue | 270 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 4329.4331 | Arrival | 272 | Host | enters the system | 0 | 1 |
| 4329.4331 | Queue | 272 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 4331.0369 | Move | 267 | Waiters | service done, routing to Cashier | 1 | 2 |
| 4331.0369 | Seize | 270 | Waiters | pulled from queue after waiting 3.3201, service until 4367.5020 | 0 | 3 |
| 4331.0369 | Seize | 267 | Cashier | server free, service 3.8090 until 4334.8460 | 0 | 1 |
| 4332.5834 | Move | 271 | Host | service done, routing to Waiters | 1 | 0 |
| 4332.5834 | Seize | 272 | Host | pulled from queue after waiting 3.1503, service until 4333.0759 | 0 | 1 |
| 4332.5834 | Queue | 271 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 4333.0759 | Move | 272 | Host | service done, routing to Waiters | 0 | 0 |
| 4333.0759 | Queue | 272 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 4334.8460 | Exit | 267 | Cashier | exits; total wait 0.0000, time in system 39.1738 | 0 | 0 |
| 4337.7070 | Arrival | 273 | Host | enters the system | 0 | 0 |
| 4337.7070 | Seize | 273 | Host | server free, service 4.5078 until 4342.2147 | 0 | 1 |
| 4342.2147 | Move | 273 | Host | service done, routing to Waiters | 0 | 0 |
| 4342.2147 | Queue | 273 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 4346.2809 | Arrival | 274 | Host | enters the system | 0 | 0 |
| 4346.2809 | Seize | 274 | Host | server free, service 1.7575 until 4348.0384 | 0 | 1 |
| 4348.0384 | Move | 274 | Host | service done, routing to Waiters | 0 | 0 |
| 4348.0384 | Queue | 274 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 4348.4136 | Arrival | 275 | Host | enters the system | 0 | 0 |
| 4348.4136 | Seize | 275 | Host | server free, service 0.3421 until 4348.7558 | 0 | 1 |
| 4348.7558 | Move | 275 | Host | service done, routing to Waiters | 0 | 0 |
| 4348.7558 | Queue | 275 | Waiters | all 3 busy, queued at position 5 | 5 | 3 |
| 4350.0189 | Move | 268 | Waiters | service done, routing to Cashier | 5 | 2 |
| 4350.0189 | Seize | 271 | Waiters | pulled from queue after waiting 17.4354, service until 4384.1215 | 4 | 3 |
| 4350.0189 | Seize | 268 | Cashier | server free, service 2.4238 until 4352.4427 | 0 | 1 |
| 4352.4427 | Exit | 268 | Cashier | exits; total wait 1.2475, time in system 55.4242 | 0 | 0 |
| 4367.5020 | Move | 270 | Waiters | service done, routing to Cashier | 4 | 2 |
| 4367.5020 | Seize | 272 | Waiters | pulled from queue after waiting 34.4261, service until 4408.4087 | 3 | 3 |
| 4367.5020 | Seize | 270 | Cashier | server free, service 3.4851 until 4370.9872 | 0 | 1 |
| 4369.6237 | Move | 269 | Waiters | service done, routing to Cashier | 3 | 2 |
| 4369.6237 | Seize | 273 | Waiters | pulled from queue after waiting 27.4089, service until 4408.8602 | 2 | 3 |
| 4369.6237 | Queue | 269 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 4370.9872 | Exit | 270 | Cashier | exits; total wait 3.4396, time in system 48.1906 | 1 | 0 |
| 4370.9872 | Seize | 269 | Cashier | pulled from queue after waiting 1.3635, service until 4372.3971 | 0 | 1 |
| 4372.3971 | Exit | 269 | Cashier | exits; total wait 1.3635, time in system 51.4455 | 0 | 0 |
| 4384.1215 | Move | 271 | Waiters | service done, routing to Cashier | 2 | 2 |
| 4384.1215 | Seize | 274 | Waiters | pulled from queue after waiting 36.0832, service until 4424.0975 | 1 | 3 |
| 4384.1215 | Seize | 271 | Cashier | server free, service 3.1101 until 4387.2316 | 0 | 1 |
| 4387.2316 | Exit | 271 | Cashier | exits; total wait 17.7459, time in system 59.8253 | 0 | 0 |
| 4388.2922 | Arrival | 276 | Host | enters the system | 0 | 0 |
| 4388.2922 | Seize | 276 | Host | server free, service 9.5318 until 4397.8241 | 0 | 1 |
| 4397.8241 | Move | 276 | Host | service done, routing to Waiters | 0 | 0 |
| 4397.8241 | Queue | 276 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 4402.7027 | Arrival | 277 | Host | enters the system | 0 | 0 |
| 4402.7027 | Seize | 277 | Host | server free, service 0.9509 until 4403.6535 | 0 | 1 |
| 4403.6535 | Move | 277 | Host | service done, routing to Waiters | 0 | 0 |
| 4403.6535 | Queue | 277 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 4408.4087 | Move | 272 | Waiters | service done, routing to Cashier | 3 | 2 |
| 4408.4087 | Seize | 275 | Waiters | pulled from queue after waiting 59.6530, service until 4433.2051 | 2 | 3 |
| 4408.4087 | Seize | 272 | Cashier | server free, service 1.4531 until 4409.8618 | 0 | 1 |
| 4408.8602 | Move | 273 | Waiters | service done, routing to Cashier | 2 | 2 |
| 4408.8602 | Seize | 276 | Waiters | pulled from queue after waiting 11.0361, service until 4443.0150 | 1 | 3 |
| 4408.8602 | Queue | 273 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 4409.8618 | Exit | 272 | Cashier | exits; total wait 37.5764, time in system 80.4287 | 1 | 0 |
| 4409.8618 | Seize | 273 | Cashier | pulled from queue after waiting 1.0016, service until 4411.9418 | 0 | 1 |
| 4411.9418 | Exit | 273 | Cashier | exits; total wait 28.4106, time in system 74.2348 | 0 | 0 |
| 4424.0975 | Move | 274 | Waiters | service done, routing to Cashier | 1 | 2 |
| 4424.0975 | Seize | 277 | Waiters | pulled from queue after waiting 20.4440, service until 4447.9324 | 0 | 3 |
| 4424.0975 | Seize | 274 | Cashier | server free, service 1.0588 until 4425.1563 | 0 | 1 |
| 4425.1563 | Exit | 274 | Cashier | exits; total wait 36.0832, time in system 78.8754 | 0 | 0 |
| 4433.2051 | Move | 275 | Waiters | service done, routing to Cashier | 0 | 2 |
| 4433.2051 | Seize | 275 | Cashier | server free, service 1.0333 until 4434.2384 | 0 | 1 |
| 4434.2384 | Exit | 275 | Cashier | exits; total wait 59.6530, time in system 85.8248 | 0 | 0 |
| 4437.6786 | Arrival | 278 | Host | enters the system | 0 | 0 |
| 4437.6786 | Seize | 278 | Host | server free, service 1.8182 until 4439.4969 | 0 | 1 |
| 4439.4969 | Move | 278 | Host | service done, routing to Waiters | 0 | 0 |
| 4439.4969 | Seize | 278 | Waiters | server free, service 35.2164 until 4474.7133 | 0 | 3 |
| 4442.2244 | Arrival | 279 | Host | enters the system | 0 | 0 |
| 4442.2244 | Seize | 279 | Host | server free, service 0.3357 until 4442.5601 | 0 | 1 |
| 4442.5601 | Move | 279 | Host | service done, routing to Waiters | 0 | 0 |
| 4442.5601 | Queue | 279 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 4443.0150 | Move | 276 | Waiters | service done, routing to Cashier | 1 | 2 |
| 4443.0150 | Seize | 279 | Waiters | pulled from queue after waiting 0.4549, service until 4474.7300 | 0 | 3 |
| 4443.0150 | Seize | 276 | Cashier | server free, service 3.4836 until 4446.4986 | 0 | 1 |
| 4446.4986 | Exit | 276 | Cashier | exits; total wait 11.0361, time in system 58.2064 | 0 | 0 |
| 4447.9324 | Move | 277 | Waiters | service done, routing to Cashier | 0 | 2 |
| 4447.9324 | Seize | 277 | Cashier | server free, service 1.6131 until 4449.5455 | 0 | 1 |
| 4449.5455 | Exit | 277 | Cashier | exits; total wait 20.4440, time in system 46.8429 | 0 | 0 |
| 4474.7133 | Move | 278 | Waiters | service done, routing to Cashier | 0 | 1 |
| 4474.7133 | Seize | 278 | Cashier | server free, service 3.7704 until 4478.4837 | 0 | 1 |
| 4474.7300 | Move | 279 | Waiters | service done, routing to Cashier | 0 | 0 |
| 4474.7300 | Queue | 279 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 4478.4837 | Exit | 278 | Cashier | exits; total wait 0.0000, time in system 40.8050 | 1 | 0 |
| 4478.4837 | Seize | 279 | Cashier | pulled from queue after waiting 3.7537, service until 4480.8053 | 0 | 1 |
| 4480.8053 | Exit | 279 | Cashier | exits; total wait 4.2085, time in system 38.5808 | 0 | 0 |
| 4489.2337 | Arrival | 280 | Host | enters the system | 0 | 0 |
| 4489.2337 | Seize | 280 | Host | server free, service 0.0784 until 4489.3121 | 0 | 1 |
| 4489.3121 | Move | 280 | Host | service done, routing to Waiters | 0 | 0 |
| 4489.3121 | Seize | 280 | Waiters | server free, service 29.8913 until 4519.2034 | 0 | 1 |
| 4508.0436 | Arrival | 281 | Host | enters the system | 0 | 0 |
| 4508.0436 | Seize | 281 | Host | server free, service 0.7369 until 4508.7805 | 0 | 1 |
| 4508.7805 | Move | 281 | Host | service done, routing to Waiters | 0 | 0 |
| 4508.7805 | Seize | 281 | Waiters | server free, service 41.7502 until 4550.5307 | 0 | 2 |
| 4519.2034 | Move | 280 | Waiters | service done, routing to Cashier | 0 | 1 |
| 4519.2034 | Seize | 280 | Cashier | server free, service 1.0033 until 4520.2067 | 0 | 1 |
| 4520.2067 | Exit | 280 | Cashier | exits; total wait 0.0000, time in system 30.9730 | 0 | 0 |
| 4530.6626 | Arrival | 282 | Host | enters the system | 0 | 0 |
| 4530.6626 | Seize | 282 | Host | server free, service 0.0519 until 4530.7145 | 0 | 1 |
| 4530.7145 | Move | 282 | Host | service done, routing to Waiters | 0 | 0 |
| 4530.7145 | Seize | 282 | Waiters | server free, service 54.5570 until 4585.2715 | 0 | 2 |
| 4535.1070 | Arrival | 283 | Host | enters the system | 0 | 0 |
| 4535.1070 | Seize | 283 | Host | server free, service 1.7466 until 4536.8536 | 0 | 1 |
| 4536.8536 | Move | 283 | Host | service done, routing to Waiters | 0 | 0 |
| 4536.8536 | Seize | 283 | Waiters | server free, service 39.3403 until 4576.1939 | 0 | 3 |
| 4549.2068 | Arrival | 284 | Host | enters the system | 0 | 0 |
| 4549.2068 | Seize | 284 | Host | server free, service 5.1477 until 4554.3545 | 0 | 1 |
| 4550.5307 | Move | 281 | Waiters | service done, routing to Cashier | 0 | 2 |
| 4550.5307 | Seize | 281 | Cashier | server free, service 2.6105 until 4553.1412 | 0 | 1 |
| 4553.1412 | Exit | 281 | Cashier | exits; total wait 0.0000, time in system 45.0976 | 0 | 0 |
| 4554.3545 | Move | 284 | Host | service done, routing to Waiters | 0 | 0 |
| 4554.3545 | Seize | 284 | Waiters | server free, service 31.2856 until 4585.6402 | 0 | 3 |
| 4565.9329 | Arrival | 285 | Host | enters the system | 0 | 0 |
| 4565.9329 | Seize | 285 | Host | server free, service 0.7446 until 4566.6775 | 0 | 1 |
| 4566.4113 | Arrival | 286 | Host | enters the system | 0 | 1 |
| 4566.4113 | Queue | 286 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 4566.6775 | Move | 285 | Host | service done, routing to Waiters | 1 | 0 |
| 4566.6775 | Seize | 286 | Host | pulled from queue after waiting 0.2662, service until 4567.0379 | 0 | 1 |
| 4566.6775 | Queue | 285 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 4567.0379 | Move | 286 | Host | service done, routing to Waiters | 0 | 0 |
| 4567.0379 | Queue | 286 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 4570.1766 | Arrival | 287 | Host | enters the system | 0 | 0 |
| 4570.1766 | Seize | 287 | Host | server free, service 0.8498 until 4571.0264 | 0 | 1 |
| 4571.0264 | Move | 287 | Host | service done, routing to Waiters | 0 | 0 |
| 4571.0264 | Queue | 287 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 4576.1939 | Move | 283 | Waiters | service done, routing to Cashier | 3 | 2 |
| 4576.1939 | Seize | 285 | Waiters | pulled from queue after waiting 9.5164, service until 4613.9369 | 2 | 3 |
| 4576.1939 | Seize | 283 | Cashier | server free, service 1.8440 until 4578.0378 | 0 | 1 |
| 4578.0378 | Exit | 283 | Cashier | exits; total wait 0.0000, time in system 42.9308 | 0 | 0 |
| 4585.2715 | Move | 282 | Waiters | service done, routing to Cashier | 2 | 2 |
| 4585.2715 | Seize | 286 | Waiters | pulled from queue after waiting 18.2337, service until 4621.8466 | 1 | 3 |
| 4585.2715 | Seize | 282 | Cashier | server free, service 3.2329 until 4588.5044 | 0 | 1 |
| 4585.6402 | Move | 284 | Waiters | service done, routing to Cashier | 1 | 2 |
| 4585.6402 | Seize | 287 | Waiters | pulled from queue after waiting 14.6138, service until 4635.6296 | 0 | 3 |
| 4585.6402 | Queue | 284 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 4588.5044 | Exit | 282 | Cashier | exits; total wait 0.0000, time in system 57.8418 | 1 | 0 |
| 4588.5044 | Seize | 284 | Cashier | pulled from queue after waiting 2.8643, service until 4592.0663 | 0 | 1 |
| 4592.0663 | Exit | 284 | Cashier | exits; total wait 2.8643, time in system 42.8595 | 0 | 0 |
| 4603.9035 | Arrival | 288 | Host | enters the system | 0 | 0 |
| 4603.9035 | Seize | 288 | Host | server free, service 0.5538 until 4604.4573 | 0 | 1 |
| 4604.4573 | Move | 288 | Host | service done, routing to Waiters | 0 | 0 |
| 4604.4573 | Queue | 288 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 4613.9369 | Move | 285 | Waiters | service done, routing to Cashier | 1 | 2 |
| 4613.9369 | Seize | 288 | Waiters | pulled from queue after waiting 9.4796, service until 4648.7221 | 0 | 3 |
| 4613.9369 | Seize | 285 | Cashier | server free, service 3.3762 until 4617.3131 | 0 | 1 |
| 4617.3131 | Exit | 285 | Cashier | exits; total wait 9.5164, time in system 51.3803 | 0 | 0 |
| 4621.8466 | Move | 286 | Waiters | service done, routing to Cashier | 0 | 2 |
| 4621.8466 | Seize | 286 | Cashier | server free, service 3.2903 until 4625.1369 | 0 | 1 |
| 4625.1369 | Exit | 286 | Cashier | exits; total wait 18.4998, time in system 58.7256 | 0 | 0 |
| 4626.7129 | Arrival | 289 | Host | enters the system | 0 | 0 |
| 4626.7129 | Seize | 289 | Host | server free, service 0.7813 until 4627.4942 | 0 | 1 |
| 4627.4942 | Move | 289 | Host | service done, routing to Waiters | 0 | 0 |
| 4627.4942 | Seize | 289 | Waiters | server free, service 52.9637 until 4680.4579 | 0 | 3 |
| 4631.9792 | Arrival | 290 | Host | enters the system | 0 | 0 |
| 4631.9792 | Seize | 290 | Host | server free, service 2.1203 until 4634.0995 | 0 | 1 |
| 4634.0995 | Move | 290 | Host | service done, routing to Waiters | 0 | 0 |
| 4634.0995 | Queue | 290 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 4635.6296 | Move | 287 | Waiters | service done, routing to Cashier | 1 | 2 |
| 4635.6296 | Seize | 290 | Waiters | pulled from queue after waiting 1.5301, service until 4688.9772 | 0 | 3 |
| 4635.6296 | Seize | 287 | Cashier | server free, service 3.7912 until 4639.4209 | 0 | 1 |
| 4639.4209 | Exit | 287 | Cashier | exits; total wait 14.6138, time in system 69.2443 | 0 | 0 |
| 4642.2089 | Arrival | 291 | Host | enters the system | 0 | 0 |
| 4642.2089 | Seize | 291 | Host | server free, service 0.9261 until 4643.1350 | 0 | 1 |
| 4643.1350 | Move | 291 | Host | service done, routing to Waiters | 0 | 0 |
| 4643.1350 | Queue | 291 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 4644.7961 | Arrival | 292 | Host | enters the system | 0 | 0 |
| 4644.7961 | Seize | 292 | Host | server free, service 2.3499 until 4647.1460 | 0 | 1 |
| 4647.1460 | Move | 292 | Host | service done, routing to Waiters | 0 | 0 |
| 4647.1460 | Queue | 292 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 4648.7221 | Move | 288 | Waiters | service done, routing to Cashier | 2 | 2 |
| 4648.7221 | Seize | 291 | Waiters | pulled from queue after waiting 5.5871, service until 4693.7834 | 1 | 3 |
| 4648.7221 | Seize | 288 | Cashier | server free, service 3.4577 until 4652.1798 | 0 | 1 |
| 4652.1798 | Exit | 288 | Cashier | exits; total wait 9.4796, time in system 48.2763 | 0 | 0 |
| 4680.4579 | Move | 289 | Waiters | service done, routing to Cashier | 1 | 2 |
| 4680.4579 | Seize | 292 | Waiters | pulled from queue after waiting 33.3119, service until 4714.4375 | 0 | 3 |
| 4680.4579 | Seize | 289 | Cashier | server free, service 2.5996 until 4683.0575 | 0 | 1 |
| 4683.0575 | Exit | 289 | Cashier | exits; total wait 0.0000, time in system 56.3446 | 0 | 0 |
| 4688.2154 | Arrival | 293 | Host | enters the system | 0 | 0 |
| 4688.2154 | Seize | 293 | Host | server free, service 2.6622 until 4690.8776 | 0 | 1 |
| 4688.9772 | Move | 290 | Waiters | service done, routing to Cashier | 0 | 2 |
| 4688.9772 | Seize | 290 | Cashier | server free, service 2.2553 until 4691.2325 | 0 | 1 |
| 4690.8776 | Move | 293 | Host | service done, routing to Waiters | 0 | 0 |
| 4690.8776 | Seize | 293 | Waiters | server free, service 32.0602 until 4722.9378 | 0 | 3 |
| 4691.2325 | Exit | 290 | Cashier | exits; total wait 1.5301, time in system 59.2532 | 0 | 0 |
| 4693.7834 | Move | 291 | Waiters | service done, routing to Cashier | 0 | 2 |
| 4693.7834 | Seize | 291 | Cashier | server free, service 1.2271 until 4695.0105 | 0 | 1 |
| 4695.0105 | Exit | 291 | Cashier | exits; total wait 5.5871, time in system 52.8016 | 0 | 0 |
| 4714.4375 | Move | 292 | Waiters | service done, routing to Cashier | 0 | 1 |
| 4714.4375 | Seize | 292 | Cashier | server free, service 2.3428 until 4716.7803 | 0 | 1 |
| 4716.7803 | Exit | 292 | Cashier | exits; total wait 33.3119, time in system 71.9843 | 0 | 0 |
| 4722.9378 | Move | 293 | Waiters | service done, routing to Cashier | 0 | 0 |
| 4722.9378 | Seize | 293 | Cashier | server free, service 1.9155 until 4724.8533 | 0 | 1 |
| 4724.8533 | Exit | 293 | Cashier | exits; total wait 0.0000, time in system 36.6379 | 0 | 0 |
| 4739.5781 | Arrival | 294 | Host | enters the system | 0 | 0 |
| 4739.5781 | Seize | 294 | Host | server free, service 8.0104 until 4747.5884 | 0 | 1 |
| 4747.5884 | Move | 294 | Host | service done, routing to Waiters | 0 | 0 |
| 4747.5884 | Seize | 294 | Waiters | server free, service 26.2357 until 4773.8242 | 0 | 1 |
| 4752.2403 | Arrival | 295 | Host | enters the system | 0 | 0 |
| 4752.2403 | Seize | 295 | Host | server free, service 0.4647 until 4752.7050 | 0 | 1 |
| 4752.7050 | Move | 295 | Host | service done, routing to Waiters | 0 | 0 |
| 4752.7050 | Seize | 295 | Waiters | server free, service 29.4369 until 4782.1419 | 0 | 2 |
| 4767.7073 | Arrival | 296 | Host | enters the system | 0 | 0 |
| 4767.7073 | Seize | 296 | Host | server free, service 0.3182 until 4768.0254 | 0 | 1 |
| 4768.0254 | Move | 296 | Host | service done, routing to Waiters | 0 | 0 |
| 4768.0254 | Seize | 296 | Waiters | server free, service 22.7324 until 4790.7578 | 0 | 3 |
| 4773.8242 | Move | 294 | Waiters | service done, routing to Cashier | 0 | 2 |
| 4773.8242 | Seize | 294 | Cashier | server free, service 2.8557 until 4776.6799 | 0 | 1 |
| 4776.6799 | Exit | 294 | Cashier | exits; total wait 0.0000, time in system 37.1018 | 0 | 0 |
| 4782.1419 | Move | 295 | Waiters | service done, routing to Cashier | 0 | 1 |
| 4782.1419 | Seize | 295 | Cashier | server free, service 3.9545 until 4786.0964 | 0 | 1 |
| 4786.0964 | Exit | 295 | Cashier | exits; total wait 0.0000, time in system 33.8561 | 0 | 0 |
| 4790.7578 | Move | 296 | Waiters | service done, routing to Cashier | 0 | 0 |
| 4790.7578 | Seize | 296 | Cashier | server free, service 3.1566 until 4793.9144 | 0 | 1 |
| 4793.9144 | Exit | 296 | Cashier | exits; total wait 0.0000, time in system 26.2071 | 0 | 0 |
| 4799.7329 | Arrival | 297 | Host | enters the system | 0 | 0 |
| 4799.7329 | Seize | 297 | Host | server free, service 4.7758 until 4804.5087 | 0 | 1 |
| 4804.5087 | Move | 297 | Host | service done, routing to Waiters | 0 | 0 |
| 4804.5087 | Seize | 297 | Waiters | server free, service 41.2097 until 4845.7184 | 0 | 1 |
| 4815.0759 | Arrival | 298 | Host | enters the system | 0 | 0 |
| 4815.0759 | Seize | 298 | Host | server free, service 1.6240 until 4816.6999 | 0 | 1 |
| 4816.6999 | Move | 298 | Host | service done, routing to Waiters | 0 | 0 |
| 4816.6999 | Seize | 298 | Waiters | server free, service 47.3341 until 4864.0339 | 0 | 2 |
| 4827.3412 | Arrival | 299 | Host | enters the system | 0 | 0 |
| 4827.3412 | Seize | 299 | Host | server free, service 0.9430 until 4828.2842 | 0 | 1 |
| 4828.2842 | Move | 299 | Host | service done, routing to Waiters | 0 | 0 |
| 4828.2842 | Seize | 299 | Waiters | server free, service 31.1542 until 4859.4384 | 0 | 3 |
| 4835.6668 | Arrival | 300 | Host | enters the system | 0 | 0 |
| 4835.6668 | Seize | 300 | Host | server free, service 1.1841 until 4836.8509 | 0 | 1 |
| 4836.8509 | Move | 300 | Host | service done, routing to Waiters | 0 | 0 |
| 4836.8509 | Queue | 300 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 4841.2700 | Arrival | 301 | Host | enters the system | 0 | 0 |
| 4841.2700 | Seize | 301 | Host | server free, service 0.6954 until 4841.9654 | 0 | 1 |
| 4841.5399 | Arrival | 302 | Host | enters the system | 0 | 1 |
| 4841.5399 | Queue | 302 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 4841.5721 | Arrival | 303 | Host | enters the system | 1 | 1 |
| 4841.5721 | Queue | 303 | Host | all 1 busy, queued at position 2 | 2 | 1 |
| 4841.9654 | Move | 301 | Host | service done, routing to Waiters | 2 | 0 |
| 4841.9654 | Seize | 302 | Host | pulled from queue after waiting 0.4255, service until 4842.8189 | 1 | 1 |
| 4841.9654 | Queue | 301 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 4842.8189 | Move | 302 | Host | service done, routing to Waiters | 1 | 0 |
| 4842.8189 | Seize | 303 | Host | pulled from queue after waiting 1.2468, service until 4844.3196 | 0 | 1 |
| 4842.8189 | Queue | 302 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 4842.8330 | Arrival | 304 | Host | enters the system | 0 | 1 |
| 4842.8330 | Queue | 304 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 4844.3196 | Move | 303 | Host | service done, routing to Waiters | 1 | 0 |
| 4844.3196 | Seize | 304 | Host | pulled from queue after waiting 1.4866, service until 4848.0335 | 0 | 1 |
| 4844.3196 | Queue | 303 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 4845.7184 | Move | 297 | Waiters | service done, routing to Cashier | 4 | 2 |
| 4845.7184 | Seize | 300 | Waiters | pulled from queue after waiting 8.8675, service until 4878.3503 | 3 | 3 |
| 4845.7184 | Seize | 297 | Cashier | server free, service 1.9671 until 4847.6856 | 0 | 1 |
| 4847.6856 | Exit | 297 | Cashier | exits; total wait 0.0000, time in system 47.9527 | 0 | 0 |
| 4848.0335 | Move | 304 | Host | service done, routing to Waiters | 0 | 0 |
| 4848.0335 | Queue | 304 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 4848.0696 | Arrival | 305 | Host | enters the system | 0 | 0 |
| 4848.0696 | Seize | 305 | Host | server free, service 1.0060 until 4849.0756 | 0 | 1 |
| 4849.0756 | Move | 305 | Host | service done, routing to Waiters | 0 | 0 |
| 4849.0756 | Queue | 305 | Waiters | all 3 busy, queued at position 5 | 5 | 3 |
| 4851.2570 | Arrival | 306 | Host | enters the system | 0 | 0 |
| 4851.2570 | Seize | 306 | Host | server free, service 0.4756 until 4851.7326 | 0 | 1 |
| 4851.7326 | Move | 306 | Host | service done, routing to Waiters | 0 | 0 |
| 4851.7326 | Queue | 306 | Waiters | all 3 busy, queued at position 6 | 6 | 3 |
| 4853.8096 | Arrival | 307 | Host | enters the system | 0 | 0 |
| 4853.8096 | Seize | 307 | Host | server free, service 0.1537 until 4853.9633 | 0 | 1 |
| 4853.9633 | Move | 307 | Host | service done, routing to Waiters | 0 | 0 |
| 4853.9633 | Queue | 307 | Waiters | all 3 busy, queued at position 7 | 7 | 3 |
| 4859.4384 | Move | 299 | Waiters | service done, routing to Cashier | 7 | 2 |
| 4859.4384 | Seize | 301 | Waiters | pulled from queue after waiting 17.4730, service until 4909.0823 | 6 | 3 |
| 4859.4384 | Seize | 299 | Cashier | server free, service 2.4582 until 4861.8966 | 0 | 1 |
| 4861.8966 | Exit | 299 | Cashier | exits; total wait 0.0000, time in system 34.5554 | 0 | 0 |
| 4861.9234 | Arrival | 308 | Host | enters the system | 0 | 0 |
| 4861.9234 | Seize | 308 | Host | server free, service 0.2253 until 4862.1487 | 0 | 1 |
| 4862.1487 | Move | 308 | Host | service done, routing to Waiters | 0 | 0 |
| 4862.1487 | Queue | 308 | Waiters | all 3 busy, queued at position 7 | 7 | 3 |
| 4864.0339 | Move | 298 | Waiters | service done, routing to Cashier | 7 | 2 |
| 4864.0339 | Seize | 302 | Waiters | pulled from queue after waiting 21.2150, service until 4903.8891 | 6 | 3 |
| 4864.0339 | Seize | 298 | Cashier | server free, service 2.1968 until 4866.2307 | 0 | 1 |
| 4866.2307 | Exit | 298 | Cashier | exits; total wait 0.0000, time in system 51.1549 | 0 | 0 |
| 4869.1507 | Arrival | 309 | Host | enters the system | 0 | 0 |
| 4869.1507 | Seize | 309 | Host | server free, service 2.1644 until 4871.3151 | 0 | 1 |
| 4871.3151 | Move | 309 | Host | service done, routing to Waiters | 0 | 0 |
| 4871.3151 | Queue | 309 | Waiters | all 3 busy, queued at position 7 | 7 | 3 |
| 4872.4795 | Arrival | 310 | Host | enters the system | 0 | 0 |
| 4872.4795 | Seize | 310 | Host | server free, service 4.9288 until 4877.4083 | 0 | 1 |
| 4877.4083 | Move | 310 | Host | service done, routing to Waiters | 0 | 0 |
| 4877.4083 | Queue | 310 | Waiters | all 3 busy, queued at position 8 | 8 | 3 |
| 4878.3503 | Move | 300 | Waiters | service done, routing to Cashier | 8 | 2 |
| 4878.3503 | Seize | 303 | Waiters | pulled from queue after waiting 34.0307, service until 4926.9940 | 7 | 3 |
| 4878.3503 | Seize | 300 | Cashier | server free, service 2.0444 until 4880.3947 | 0 | 1 |
| 4880.3947 | Exit | 300 | Cashier | exits; total wait 8.8675, time in system 44.7279 | 0 | 0 |
| 4882.2418 | Arrival | 311 | Host | enters the system | 0 | 0 |
| 4882.2418 | Seize | 311 | Host | server free, service 4.8637 until 4887.1055 | 0 | 1 |
| 4887.1055 | Move | 311 | Host | service done, routing to Waiters | 0 | 0 |
| 4887.1055 | Queue | 311 | Waiters | all 3 busy, queued at position 8 | 8 | 3 |
| 4889.9732 | Arrival | 312 | Host | enters the system | 0 | 0 |
| 4889.9732 | Seize | 312 | Host | server free, service 3.1279 until 4893.1011 | 0 | 1 |
| 4893.1011 | Move | 312 | Host | service done, routing to Waiters | 0 | 0 |
| 4893.1011 | Queue | 312 | Waiters | all 3 busy, queued at position 9 | 9 | 3 |
| 4903.8891 | Move | 302 | Waiters | service done, routing to Cashier | 9 | 2 |
| 4903.8891 | Seize | 304 | Waiters | pulled from queue after waiting 55.8555, service until 4930.4845 | 8 | 3 |
| 4903.8891 | Seize | 302 | Cashier | server free, service 1.1231 until 4905.0122 | 0 | 1 |
| 4905.0122 | Exit | 302 | Cashier | exits; total wait 21.6405, time in system 63.4723 | 0 | 0 |
| 4909.0823 | Move | 301 | Waiters | service done, routing to Cashier | 8 | 2 |
| 4909.0823 | Seize | 305 | Waiters | pulled from queue after waiting 60.0067, service until 4949.5281 | 7 | 3 |
| 4909.0823 | Seize | 301 | Cashier | server free, service 3.2641 until 4912.3464 | 0 | 1 |
| 4912.3464 | Exit | 301 | Cashier | exits; total wait 17.4730, time in system 71.0763 | 0 | 0 |
| 4926.9940 | Move | 303 | Waiters | service done, routing to Cashier | 7 | 2 |
| 4926.9940 | Seize | 306 | Waiters | pulled from queue after waiting 75.2614, service until 4969.8942 | 6 | 3 |
| 4926.9940 | Seize | 303 | Cashier | server free, service 3.3408 until 4930.3348 | 0 | 1 |
| 4930.3348 | Exit | 303 | Cashier | exits; total wait 35.2775, time in system 88.7627 | 0 | 0 |
| 4930.4845 | Move | 304 | Waiters | service done, routing to Cashier | 6 | 2 |
| 4930.4845 | Seize | 307 | Waiters | pulled from queue after waiting 76.5212, service until 4960.4508 | 5 | 3 |
| 4930.4845 | Seize | 304 | Cashier | server free, service 1.0598 until 4931.5443 | 0 | 1 |
| 4931.5443 | Exit | 304 | Cashier | exits; total wait 57.3422, time in system 88.7113 | 0 | 0 |
| 4940.1108 | Arrival | 313 | Host | enters the system | 0 | 0 |
| 4940.1108 | Seize | 313 | Host | server free, service 4.4533 until 4944.5641 | 0 | 1 |
| 4944.5641 | Move | 313 | Host | service done, routing to Waiters | 0 | 0 |
| 4944.5641 | Queue | 313 | Waiters | all 3 busy, queued at position 6 | 6 | 3 |
| 4949.5281 | Move | 305 | Waiters | service done, routing to Cashier | 6 | 2 |
| 4949.5281 | Seize | 308 | Waiters | pulled from queue after waiting 87.3794, service until 4981.7849 | 5 | 3 |
| 4949.5281 | Seize | 305 | Cashier | server free, service 1.5600 until 4951.0881 | 0 | 1 |
| 4951.0881 | Exit | 305 | Cashier | exits; total wait 60.0067, time in system 103.0185 | 0 | 0 |
| 4952.4238 | Arrival | 314 | Host | enters the system | 0 | 0 |
| 4952.4238 | Seize | 314 | Host | server free, service 0.8314 until 4953.2553 | 0 | 1 |
| 4953.2553 | Move | 314 | Host | service done, routing to Waiters | 0 | 0 |
| 4953.2553 | Queue | 314 | Waiters | all 3 busy, queued at position 6 | 6 | 3 |
| 4960.4508 | Move | 307 | Waiters | service done, routing to Cashier | 6 | 2 |
| 4960.4508 | Seize | 309 | Waiters | pulled from queue after waiting 89.1357, service until 4995.0039 | 5 | 3 |
| 4960.4508 | Seize | 307 | Cashier | server free, service 2.8267 until 4963.2775 | 0 | 1 |
| 4963.2775 | Exit | 307 | Cashier | exits; total wait 76.5212, time in system 109.4679 | 0 | 0 |
| 4969.8942 | Move | 306 | Waiters | service done, routing to Cashier | 5 | 2 |
| 4969.8942 | Seize | 310 | Waiters | pulled from queue after waiting 92.4859, service until 5002.5788 | 4 | 3 |
| 4969.8942 | Seize | 306 | Cashier | server free, service 3.8146 until 4973.7088 | 0 | 1 |
| 4973.7088 | Exit | 306 | Cashier | exits; total wait 75.2614, time in system 122.4518 | 0 | 0 |
| 4981.7849 | Move | 308 | Waiters | service done, routing to Cashier | 4 | 2 |
| 4981.7849 | Seize | 311 | Waiters | pulled from queue after waiting 94.6794, service until 5012.4670 | 3 | 3 |
| 4981.7849 | Seize | 308 | Cashier | server free, service 1.0649 until 4982.8498 | 0 | 1 |
| 4982.8498 | Exit | 308 | Cashier | exits; total wait 87.3794, time in system 120.9264 | 0 | 0 |
| 4984.4292 | Arrival | 315 | Host | enters the system | 0 | 0 |
| 4984.4292 | Seize | 315 | Host | server free, service 1.7718 until 4986.2010 | 0 | 1 |
| 4986.2010 | Move | 315 | Host | service done, routing to Waiters | 0 | 0 |
| 4986.2010 | Queue | 315 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 4993.3556 | Arrival | 316 | Host | enters the system | 0 | 0 |
| 4993.3556 | Seize | 316 | Host | server free, service 2.8450 until 4996.2006 | 0 | 1 |
| 4995.0039 | Move | 309 | Waiters | service done, routing to Cashier | 4 | 2 |
| 4995.0039 | Seize | 312 | Waiters | pulled from queue after waiting 101.9027, service until 5021.2217 | 3 | 3 |
| 4995.0039 | Seize | 309 | Cashier | server free, service 1.9675 until 4996.9714 | 0 | 1 |
| 4996.2006 | Move | 316 | Host | service done, routing to Waiters | 0 | 0 |
| 4996.2006 | Queue | 316 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 4996.9714 | Exit | 309 | Cashier | exits; total wait 89.1357, time in system 127.8207 | 0 | 0 |
| 4998.2539 | Arrival | 317 | Host | enters the system | 0 | 0 |
| 4998.2539 | Seize | 317 | Host | server free, service 5.9382 until 5004.1921 | 0 | 1 |
| 5002.5788 | Move | 310 | Waiters | service done, routing to Cashier | 4 | 2 |
| 5002.5788 | Seize | 313 | Waiters | pulled from queue after waiting 58.0147, service until 5031.5520 | 3 | 3 |
| 5002.5788 | Seize | 310 | Cashier | server free, service 1.4374 until 5004.0162 | 0 | 1 |
| 5004.0162 | Exit | 310 | Cashier | exits; total wait 92.4859, time in system 131.5366 | 0 | 0 |
| 5004.1921 | Move | 317 | Host | service done, routing to Waiters | 0 | 0 |
| 5004.1921 | Queue | 317 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 5007.1896 | Arrival | 318 | Host | enters the system | 0 | 0 |
| 5007.1896 | Seize | 318 | Host | server free, service 2.8020 until 5009.9916 | 0 | 1 |
| 5009.9916 | Move | 318 | Host | service done, routing to Waiters | 0 | 0 |
| 5009.9916 | Queue | 318 | Waiters | all 3 busy, queued at position 5 | 5 | 3 |
| 5010.6008 | Arrival | 319 | Host | enters the system | 0 | 0 |
| 5010.6008 | Seize | 319 | Host | server free, service 0.7673 until 5011.3680 | 0 | 1 |
| 5011.3680 | Move | 319 | Host | service done, routing to Waiters | 0 | 0 |
| 5011.3680 | Queue | 319 | Waiters | all 3 busy, queued at position 6 | 6 | 3 |
| 5012.4670 | Move | 311 | Waiters | service done, routing to Cashier | 6 | 2 |
| 5012.4670 | Seize | 314 | Waiters | pulled from queue after waiting 59.2117, service until 5051.5887 | 5 | 3 |
| 5012.4670 | Seize | 311 | Cashier | server free, service 2.2488 until 5014.7157 | 0 | 1 |
| 5014.7157 | Exit | 311 | Cashier | exits; total wait 94.6794, time in system 132.4739 | 0 | 0 |
| 5015.8611 | Arrival | 320 | Host | enters the system | 0 | 0 |
| 5015.8611 | Seize | 320 | Host | server free, service 1.3886 until 5017.2497 | 0 | 1 |
| 5017.2497 | Move | 320 | Host | service done, routing to Waiters | 0 | 0 |
| 5017.2497 | Queue | 320 | Waiters | all 3 busy, queued at position 6 | 6 | 3 |
| 5021.2217 | Move | 312 | Waiters | service done, routing to Cashier | 6 | 2 |
| 5021.2217 | Seize | 315 | Waiters | pulled from queue after waiting 35.0207, service until 5064.1176 | 5 | 3 |
| 5021.2217 | Seize | 312 | Cashier | server free, service 3.9599 until 5025.1816 | 0 | 1 |
| 5025.1816 | Exit | 312 | Cashier | exits; total wait 101.9027, time in system 135.2084 | 0 | 0 |
| 5030.2262 | Arrival | 321 | Host | enters the system | 0 | 0 |
| 5030.2262 | Seize | 321 | Host | server free, service 0.4824 until 5030.7086 | 0 | 1 |
| 5030.7086 | Move | 321 | Host | service done, routing to Waiters | 0 | 0 |
| 5030.7086 | Queue | 321 | Waiters | all 3 busy, queued at position 6 | 6 | 3 |
| 5031.5520 | Move | 313 | Waiters | service done, routing to Cashier | 6 | 2 |
| 5031.5520 | Seize | 316 | Waiters | pulled from queue after waiting 35.3515, service until 5060.9230 | 5 | 3 |
| 5031.5520 | Seize | 313 | Cashier | server free, service 2.1851 until 5033.7371 | 0 | 1 |
| 5033.7371 | Exit | 313 | Cashier | exits; total wait 58.0147, time in system 93.6263 | 0 | 0 |
| 5036.5844 | Arrival | 322 | Host | enters the system | 0 | 0 |
| 5036.5844 | Seize | 322 | Host | server free, service 0.4591 until 5037.0436 | 0 | 1 |
| 5037.0436 | Move | 322 | Host | service done, routing to Waiters | 0 | 0 |
| 5037.0436 | Queue | 322 | Waiters | all 3 busy, queued at position 6 | 6 | 3 |
| 5051.5887 | Move | 314 | Waiters | service done, routing to Cashier | 6 | 2 |
| 5051.5887 | Seize | 317 | Waiters | pulled from queue after waiting 47.3966, service until 5086.2038 | 5 | 3 |
| 5051.5887 | Seize | 314 | Cashier | server free, service 3.6049 until 5055.1936 | 0 | 1 |
| 5055.1936 | Exit | 314 | Cashier | exits; total wait 59.2117, time in system 102.7698 | 0 | 0 |
| 5060.9230 | Move | 316 | Waiters | service done, routing to Cashier | 5 | 2 |
| 5060.9230 | Seize | 318 | Waiters | pulled from queue after waiting 50.9314, service until 5109.7409 | 4 | 3 |
| 5060.9230 | Seize | 316 | Cashier | server free, service 2.8443 until 5063.7674 | 0 | 1 |
| 5063.7674 | Exit | 316 | Cashier | exits; total wait 35.3515, time in system 70.4118 | 0 | 0 |
| 5064.1176 | Move | 315 | Waiters | service done, routing to Cashier | 4 | 2 |
| 5064.1176 | Seize | 319 | Waiters | pulled from queue after waiting 52.7496, service until 5108.4759 | 3 | 3 |
| 5064.1176 | Seize | 315 | Cashier | server free, service 3.6041 until 5067.7218 | 0 | 1 |
| 5067.7218 | Exit | 315 | Cashier | exits; total wait 35.0207, time in system 83.2925 | 0 | 0 |
| 5073.5114 | Arrival | 323 | Host | enters the system | 0 | 0 |
| 5073.5114 | Seize | 323 | Host | server free, service 1.0393 until 5074.5506 | 0 | 1 |
| 5074.5506 | Move | 323 | Host | service done, routing to Waiters | 0 | 0 |
| 5074.5506 | Queue | 323 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 5074.8705 | Arrival | 324 | Host | enters the system | 0 | 0 |
| 5074.8705 | Seize | 324 | Host | server free, service 1.3952 until 5076.2658 | 0 | 1 |
| 5076.2658 | Move | 324 | Host | service done, routing to Waiters | 0 | 0 |
| 5076.2658 | Queue | 324 | Waiters | all 3 busy, queued at position 5 | 5 | 3 |
| 5086.2038 | Move | 317 | Waiters | service done, routing to Cashier | 5 | 2 |
| 5086.2038 | Seize | 320 | Waiters | pulled from queue after waiting 68.9542, service until 5117.0729 | 4 | 3 |
| 5086.2038 | Seize | 317 | Cashier | server free, service 2.4140 until 5088.6178 | 0 | 1 |
| 5088.6178 | Exit | 317 | Cashier | exits; total wait 47.3966, time in system 90.3639 | 0 | 0 |
| 5108.4759 | Move | 319 | Waiters | service done, routing to Cashier | 4 | 2 |
| 5108.4759 | Seize | 321 | Waiters | pulled from queue after waiting 77.7672, service until 5154.2962 | 3 | 3 |
| 5108.4759 | Seize | 319 | Cashier | server free, service 3.8809 until 5112.3567 | 0 | 1 |
| 5109.7409 | Move | 318 | Waiters | service done, routing to Cashier | 3 | 2 |
| 5109.7409 | Seize | 322 | Waiters | pulled from queue after waiting 72.6974, service until 5156.8880 | 2 | 3 |
| 5109.7409 | Queue | 318 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 5111.0070 | Arrival | 325 | Host | enters the system | 0 | 0 |
| 5111.0070 | Seize | 325 | Host | server free, service 2.2693 until 5113.2763 | 0 | 1 |
| 5112.3567 | Exit | 319 | Cashier | exits; total wait 52.7496, time in system 101.7560 | 1 | 0 |
| 5112.3567 | Seize | 318 | Cashier | pulled from queue after waiting 2.6158, service until 5113.7321 | 0 | 1 |
| 5113.2763 | Move | 325 | Host | service done, routing to Waiters | 0 | 0 |
| 5113.2763 | Queue | 325 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 5113.7321 | Exit | 318 | Cashier | exits; total wait 53.5472, time in system 106.5425 | 0 | 0 |
| 5117.0729 | Move | 320 | Waiters | service done, routing to Cashier | 3 | 2 |
| 5117.0729 | Seize | 323 | Waiters | pulled from queue after waiting 42.5223, service until 5171.1190 | 2 | 3 |
| 5117.0729 | Seize | 320 | Cashier | server free, service 2.9052 until 5119.9781 | 0 | 1 |
| 5119.9781 | Exit | 320 | Cashier | exits; total wait 68.9542, time in system 104.1170 | 0 | 0 |
| 5122.4466 | Arrival | 326 | Host | enters the system | 0 | 0 |
| 5122.4466 | Seize | 326 | Host | server free, service 4.0092 until 5126.4558 | 0 | 1 |
| 5122.6974 | Arrival | 327 | Host | enters the system | 0 | 1 |
| 5122.6974 | Queue | 327 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 5124.7670 | Arrival | 328 | Host | enters the system | 1 | 1 |
| 5124.7670 | Queue | 328 | Host | all 1 busy, queued at position 2 | 2 | 1 |
| 5126.4558 | Move | 326 | Host | service done, routing to Waiters | 2 | 0 |
| 5126.4558 | Seize | 327 | Host | pulled from queue after waiting 3.7584, service until 5127.0055 | 1 | 1 |
| 5126.4558 | Queue | 326 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 5127.0055 | Move | 327 | Host | service done, routing to Waiters | 1 | 0 |
| 5127.0055 | Seize | 328 | Host | pulled from queue after waiting 2.2385, service until 5127.7779 | 0 | 1 |
| 5127.0055 | Queue | 327 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 5127.7779 | Move | 328 | Host | service done, routing to Waiters | 0 | 0 |
| 5127.7779 | Queue | 328 | Waiters | all 3 busy, queued at position 5 | 5 | 3 |
| 5134.9333 | Arrival | 329 | Host | enters the system | 0 | 0 |
| 5134.9333 | Seize | 329 | Host | server free, service 0.0234 until 5134.9567 | 0 | 1 |
| 5134.9567 | Move | 329 | Host | service done, routing to Waiters | 0 | 0 |
| 5134.9567 | Queue | 329 | Waiters | all 3 busy, queued at position 6 | 6 | 3 |
| 5151.6662 | Arrival | 330 | Host | enters the system | 0 | 0 |
| 5151.6662 | Seize | 330 | Host | server free, service 0.4773 until 5152.1435 | 0 | 1 |
| 5152.1435 | Move | 330 | Host | service done, routing to Waiters | 0 | 0 |
| 5152.1435 | Queue | 330 | Waiters | all 3 busy, queued at position 7 | 7 | 3 |
| 5154.2962 | Move | 321 | Waiters | service done, routing to Cashier | 7 | 2 |
| 5154.2962 | Seize | 324 | Waiters | pulled from queue after waiting 78.0304, service until 5203.0670 | 6 | 3 |
| 5154.2962 | Seize | 321 | Cashier | server free, service 1.9751 until 5156.2713 | 0 | 1 |
| 5156.2713 | Exit | 321 | Cashier | exits; total wait 77.7672, time in system 126.0451 | 0 | 0 |
| 5156.8880 | Move | 322 | Waiters | service done, routing to Cashier | 6 | 2 |
| 5156.8880 | Seize | 325 | Waiters | pulled from queue after waiting 43.6118, service until 5191.2660 | 5 | 3 |
| 5156.8880 | Seize | 322 | Cashier | server free, service 2.2319 until 5159.1199 | 0 | 1 |
| 5159.1199 | Exit | 322 | Cashier | exits; total wait 72.6974, time in system 122.5355 | 0 | 0 |
| 5162.7436 | Arrival | 331 | Host | enters the system | 0 | 0 |
| 5162.7436 | Seize | 331 | Host | server free, service 1.4198 until 5164.1634 | 0 | 1 |
| 5164.1634 | Move | 331 | Host | service done, routing to Waiters | 0 | 0 |
| 5164.1634 | Queue | 331 | Waiters | all 3 busy, queued at position 6 | 6 | 3 |
| 5171.1190 | Move | 323 | Waiters | service done, routing to Cashier | 6 | 2 |
| 5171.1190 | Seize | 326 | Waiters | pulled from queue after waiting 44.6631, service until 5214.4144 | 5 | 3 |
| 5171.1190 | Seize | 323 | Cashier | server free, service 3.2574 until 5174.3764 | 0 | 1 |
| 5174.3764 | Exit | 323 | Cashier | exits; total wait 42.5223, time in system 100.8650 | 0 | 0 |
| 5186.0229 | Arrival | 332 | Host | enters the system | 0 | 0 |
| 5186.0229 | Seize | 332 | Host | server free, service 0.4814 until 5186.5043 | 0 | 1 |
| 5186.5043 | Move | 332 | Host | service done, routing to Waiters | 0 | 0 |
| 5186.5043 | Queue | 332 | Waiters | all 3 busy, queued at position 6 | 6 | 3 |
| 5191.2660 | Move | 325 | Waiters | service done, routing to Cashier | 6 | 2 |
| 5191.2660 | Seize | 327 | Waiters | pulled from queue after waiting 64.2605, service until 5228.5766 | 5 | 3 |
| 5191.2660 | Seize | 325 | Cashier | server free, service 2.7332 until 5193.9992 | 0 | 1 |
| 5192.1373 | Arrival | 333 | Host | enters the system | 0 | 0 |
| 5192.1373 | Seize | 333 | Host | server free, service 0.1493 until 5192.2865 | 0 | 1 |
| 5192.2865 | Move | 333 | Host | service done, routing to Waiters | 0 | 0 |
| 5192.2865 | Queue | 333 | Waiters | all 3 busy, queued at position 6 | 6 | 3 |
| 5192.4873 | Arrival | 334 | Host | enters the system | 0 | 0 |
| 5192.4873 | Seize | 334 | Host | server free, service 7.0183 until 5199.5056 | 0 | 1 |
| 5193.9992 | Exit | 325 | Cashier | exits; total wait 43.6118, time in system 82.9922 | 0 | 0 |
| 5199.5056 | Move | 334 | Host | service done, routing to Waiters | 0 | 0 |
| 5199.5056 | Queue | 334 | Waiters | all 3 busy, queued at position 7 | 7 | 3 |
| 5203.0670 | Move | 324 | Waiters | service done, routing to Cashier | 7 | 2 |
| 5203.0670 | Seize | 328 | Waiters | pulled from queue after waiting 75.2891, service until 5255.3041 | 6 | 3 |
| 5203.0670 | Seize | 324 | Cashier | server free, service 1.7903 until 5204.8573 | 0 | 1 |
| 5204.8573 | Exit | 324 | Cashier | exits; total wait 78.0304, time in system 129.9868 | 0 | 0 |
| 5209.4452 | Arrival | 335 | Host | enters the system | 0 | 0 |
| 5209.4452 | Seize | 335 | Host | server free, service 1.9234 until 5211.3685 | 0 | 1 |
| 5211.3685 | Move | 335 | Host | service done, routing to Waiters | 0 | 0 |
| 5211.3685 | Queue | 335 | Waiters | all 3 busy, queued at position 7 | 7 | 3 |
| 5214.4144 | Move | 326 | Waiters | service done, routing to Cashier | 7 | 2 |
| 5214.4144 | Seize | 329 | Waiters | pulled from queue after waiting 79.4577, service until 5256.1407 | 6 | 3 |
| 5214.4144 | Seize | 326 | Cashier | server free, service 3.5195 until 5217.9339 | 0 | 1 |
| 5217.9339 | Exit | 326 | Cashier | exits; total wait 44.6631, time in system 95.4873 | 0 | 0 |
| 5228.5766 | Move | 327 | Waiters | service done, routing to Cashier | 6 | 2 |
| 5228.5766 | Seize | 330 | Waiters | pulled from queue after waiting 76.4331, service until 5263.8013 | 5 | 3 |
| 5228.5766 | Seize | 327 | Cashier | server free, service 3.5256 until 5232.1022 | 0 | 1 |
| 5232.1022 | Exit | 327 | Cashier | exits; total wait 68.0189, time in system 109.4048 | 0 | 0 |
| 5238.0321 | Arrival | 336 | Host | enters the system | 0 | 0 |
| 5238.0321 | Seize | 336 | Host | server free, service 0.2677 until 5238.2998 | 0 | 1 |
| 5238.2998 | Move | 336 | Host | service done, routing to Waiters | 0 | 0 |
| 5238.2998 | Queue | 336 | Waiters | all 3 busy, queued at position 6 | 6 | 3 |
| 5255.3041 | Move | 328 | Waiters | service done, routing to Cashier | 6 | 2 |
| 5255.3041 | Seize | 331 | Waiters | pulled from queue after waiting 91.1407, service until 5294.8151 | 5 | 3 |
| 5255.3041 | Seize | 328 | Cashier | server free, service 2.3081 until 5257.6122 | 0 | 1 |
| 5256.1407 | Move | 329 | Waiters | service done, routing to Cashier | 5 | 2 |
| 5256.1407 | Seize | 332 | Waiters | pulled from queue after waiting 69.6364, service until 5304.2039 | 4 | 3 |
| 5256.1407 | Queue | 329 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 5257.6122 | Exit | 328 | Cashier | exits; total wait 77.5276, time in system 132.8453 | 1 | 0 |
| 5257.6122 | Seize | 329 | Cashier | pulled from queue after waiting 1.4715, service until 5259.4100 | 0 | 1 |
| 5259.4100 | Exit | 329 | Cashier | exits; total wait 80.9293, time in system 124.4767 | 0 | 0 |
| 5263.6022 | Arrival | 337 | Host | enters the system | 0 | 0 |
| 5263.6022 | Seize | 337 | Host | server free, service 0.7647 until 5264.3669 | 0 | 1 |
| 5263.8013 | Move | 330 | Waiters | service done, routing to Cashier | 4 | 2 |
| 5263.8013 | Seize | 333 | Waiters | pulled from queue after waiting 71.5147, service until 5318.0708 | 3 | 3 |
| 5263.8013 | Seize | 330 | Cashier | server free, service 1.4191 until 5265.2203 | 0 | 1 |
| 5264.3669 | Move | 337 | Host | service done, routing to Waiters | 0 | 0 |
| 5264.3669 | Queue | 337 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 5265.2203 | Exit | 330 | Cashier | exits; total wait 76.4331, time in system 113.5541 | 0 | 0 |
| 5266.2994 | Arrival | 338 | Host | enters the system | 0 | 0 |
| 5266.2994 | Seize | 338 | Host | server free, service 0.4268 until 5266.7262 | 0 | 1 |
| 5266.7262 | Move | 338 | Host | service done, routing to Waiters | 0 | 0 |
| 5266.7262 | Queue | 338 | Waiters | all 3 busy, queued at position 5 | 5 | 3 |
| 5289.5333 | Arrival | 339 | Host | enters the system | 0 | 0 |
| 5289.5333 | Seize | 339 | Host | server free, service 4.0108 until 5293.5441 | 0 | 1 |
| 5293.1237 | Arrival | 340 | Host | enters the system | 0 | 1 |
| 5293.1237 | Queue | 340 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 5293.5441 | Move | 339 | Host | service done, routing to Waiters | 1 | 0 |
| 5293.5441 | Seize | 340 | Host | pulled from queue after waiting 0.4204, service until 5293.9608 | 0 | 1 |
| 5293.5441 | Queue | 339 | Waiters | all 3 busy, queued at position 6 | 6 | 3 |
| 5293.9608 | Move | 340 | Host | service done, routing to Waiters | 0 | 0 |
| 5293.9608 | Queue | 340 | Waiters | all 3 busy, queued at position 7 | 7 | 3 |
| 5294.8151 | Move | 331 | Waiters | service done, routing to Cashier | 7 | 2 |
| 5294.8151 | Seize | 334 | Waiters | pulled from queue after waiting 95.3096, service until 5326.2815 | 6 | 3 |
| 5294.8151 | Seize | 331 | Cashier | server free, service 2.6697 until 5297.4849 | 0 | 1 |
| 5297.4849 | Exit | 331 | Cashier | exits; total wait 91.1407, time in system 134.7413 | 0 | 0 |
| 5304.2039 | Move | 332 | Waiters | service done, routing to Cashier | 6 | 2 |
| 5304.2039 | Seize | 335 | Waiters | pulled from queue after waiting 92.8354, service until 5337.1202 | 5 | 3 |
| 5304.2039 | Seize | 332 | Cashier | server free, service 2.3238 until 5306.5277 | 0 | 1 |
| 5306.5277 | Exit | 332 | Cashier | exits; total wait 69.6364, time in system 120.5048 | 0 | 0 |
| 5306.8027 | Arrival | 341 | Host | enters the system | 0 | 0 |
| 5306.8027 | Seize | 341 | Host | server free, service 1.3217 until 5308.1244 | 0 | 1 |
| 5308.1244 | Move | 341 | Host | service done, routing to Waiters | 0 | 0 |
| 5308.1244 | Queue | 341 | Waiters | all 3 busy, queued at position 6 | 6 | 3 |
| 5318.0708 | Move | 333 | Waiters | service done, routing to Cashier | 6 | 2 |
| 5318.0708 | Seize | 336 | Waiters | pulled from queue after waiting 79.7710, service until 5346.1142 | 5 | 3 |
| 5318.0708 | Seize | 333 | Cashier | server free, service 2.5964 until 5320.6672 | 0 | 1 |
| 5320.6672 | Exit | 333 | Cashier | exits; total wait 71.5147, time in system 128.5300 | 0 | 0 |
| 5326.2815 | Move | 334 | Waiters | service done, routing to Cashier | 5 | 2 |
| 5326.2815 | Seize | 337 | Waiters | pulled from queue after waiting 61.9146, service until 5380.4627 | 4 | 3 |
| 5326.2815 | Seize | 334 | Cashier | server free, service 3.4170 until 5329.6985 | 0 | 1 |
| 5329.6985 | Exit | 334 | Cashier | exits; total wait 95.3096, time in system 137.2112 | 0 | 0 |
| 5334.1416 | Arrival | 342 | Host | enters the system | 0 | 0 |
| 5334.1416 | Seize | 342 | Host | server free, service 1.6915 until 5335.8331 | 0 | 1 |
| 5335.8331 | Move | 342 | Host | service done, routing to Waiters | 0 | 0 |
| 5335.8331 | Queue | 342 | Waiters | all 3 busy, queued at position 5 | 5 | 3 |
| 5337.1202 | Move | 335 | Waiters | service done, routing to Cashier | 5 | 2 |
| 5337.1202 | Seize | 338 | Waiters | pulled from queue after waiting 70.3940, service until 5387.5825 | 4 | 3 |
| 5337.1202 | Seize | 335 | Cashier | server free, service 3.1073 until 5340.2275 | 0 | 1 |
| 5340.2275 | Exit | 335 | Cashier | exits; total wait 92.8354, time in system 130.7824 | 0 | 0 |
| 5346.1142 | Move | 336 | Waiters | service done, routing to Cashier | 4 | 2 |
| 5346.1142 | Seize | 339 | Waiters | pulled from queue after waiting 52.5701, service until 5373.7530 | 3 | 3 |
| 5346.1142 | Seize | 336 | Cashier | server free, service 2.9994 until 5349.1136 | 0 | 1 |
| 5349.1136 | Exit | 336 | Cashier | exits; total wait 79.7710, time in system 111.0815 | 0 | 0 |
| 5359.0116 | Arrival | 343 | Host | enters the system | 0 | 0 |
| 5359.0116 | Seize | 343 | Host | server free, service 0.4800 until 5359.4916 | 0 | 1 |
| 5359.4916 | Move | 343 | Host | service done, routing to Waiters | 0 | 0 |
| 5359.4916 | Queue | 343 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 5368.9191 | Arrival | 344 | Host | enters the system | 0 | 0 |
| 5368.9191 | Seize | 344 | Host | server free, service 0.3525 until 5369.2716 | 0 | 1 |
| 5368.9429 | Arrival | 345 | Host | enters the system | 0 | 1 |
| 5368.9429 | Queue | 345 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 5369.2716 | Move | 344 | Host | service done, routing to Waiters | 1 | 0 |
| 5369.2716 | Seize | 345 | Host | pulled from queue after waiting 0.3287, service until 5369.8032 | 0 | 1 |
| 5369.2716 | Queue | 344 | Waiters | all 3 busy, queued at position 5 | 5 | 3 |
| 5369.8032 | Move | 345 | Host | service done, routing to Waiters | 0 | 0 |
| 5369.8032 | Queue | 345 | Waiters | all 3 busy, queued at position 6 | 6 | 3 |
| 5373.7530 | Move | 339 | Waiters | service done, routing to Cashier | 6 | 2 |
| 5373.7530 | Seize | 340 | Waiters | pulled from queue after waiting 79.7922, service until 5410.3979 | 5 | 3 |
| 5373.7530 | Seize | 339 | Cashier | server free, service 3.0262 until 5376.7792 | 0 | 1 |
| 5376.7792 | Exit | 339 | Cashier | exits; total wait 52.5701, time in system 87.2459 | 0 | 0 |
| 5380.4627 | Move | 337 | Waiters | service done, routing to Cashier | 5 | 2 |
| 5380.4627 | Seize | 341 | Waiters | pulled from queue after waiting 72.3382, service until 5430.4785 | 4 | 3 |
| 5380.4627 | Seize | 337 | Cashier | server free, service 2.3083 until 5382.7710 | 0 | 1 |
| 5382.7710 | Exit | 337 | Cashier | exits; total wait 61.9146, time in system 119.1687 | 0 | 0 |
| 5387.5825 | Move | 338 | Waiters | service done, routing to Cashier | 4 | 2 |
| 5387.5825 | Seize | 342 | Waiters | pulled from queue after waiting 51.7494, service until 5430.1805 | 3 | 3 |
| 5387.5825 | Seize | 338 | Cashier | server free, service 3.2115 until 5390.7941 | 0 | 1 |
| 5390.7941 | Exit | 338 | Cashier | exits; total wait 70.3940, time in system 124.4947 | 0 | 0 |
| 5391.4582 | Arrival | 346 | Host | enters the system | 0 | 0 |
| 5391.4582 | Seize | 346 | Host | server free, service 1.2284 until 5392.6865 | 0 | 1 |
| 5392.6865 | Move | 346 | Host | service done, routing to Waiters | 0 | 0 |
| 5392.6865 | Queue | 346 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 5392.7261 | Arrival | 347 | Host | enters the system | 0 | 0 |
| 5392.7261 | Seize | 347 | Host | server free, service 3.0594 until 5395.7855 | 0 | 1 |
| 5393.9636 | Arrival | 348 | Host | enters the system | 0 | 1 |
| 5393.9636 | Queue | 348 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 5395.7855 | Move | 347 | Host | service done, routing to Waiters | 1 | 0 |
| 5395.7855 | Seize | 348 | Host | pulled from queue after waiting 1.8219, service until 5400.4184 | 0 | 1 |
| 5395.7855 | Queue | 347 | Waiters | all 3 busy, queued at position 5 | 5 | 3 |
| 5400.0927 | Arrival | 349 | Host | enters the system | 0 | 1 |
| 5400.0927 | Queue | 349 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 5400.4184 | Move | 348 | Host | service done, routing to Waiters | 1 | 0 |
| 5400.4184 | Seize | 349 | Host | pulled from queue after waiting 0.3258, service until 5405.4625 | 0 | 1 |
| 5400.4184 | Queue | 348 | Waiters | all 3 busy, queued at position 6 | 6 | 3 |
| 5405.4625 | Move | 349 | Host | service done, routing to Waiters | 0 | 0 |
| 5405.4625 | Queue | 349 | Waiters | all 3 busy, queued at position 7 | 7 | 3 |
| 5410.3979 | Move | 340 | Waiters | service done, routing to Cashier | 7 | 2 |
| 5410.3979 | Seize | 343 | Waiters | pulled from queue after waiting 50.9063, service until 5458.6958 | 6 | 3 |
| 5410.3979 | Seize | 340 | Cashier | server free, service 3.5677 until 5413.9656 | 0 | 1 |
| 5413.9656 | Exit | 340 | Cashier | exits; total wait 80.2126, time in system 120.8419 | 0 | 0 |
| 5430.1805 | Move | 342 | Waiters | service done, routing to Cashier | 6 | 2 |
| 5430.1805 | Seize | 344 | Waiters | pulled from queue after waiting 60.9088, service until 5456.6809 | 5 | 3 |
| 5430.1805 | Seize | 342 | Cashier | server free, service 2.0869 until 5432.2674 | 0 | 1 |
| 5430.4785 | Move | 341 | Waiters | service done, routing to Cashier | 5 | 2 |
| 5430.4785 | Seize | 345 | Waiters | pulled from queue after waiting 60.6753, service until 5487.1226 | 4 | 3 |
| 5430.4785 | Queue | 341 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 5432.2674 | Exit | 342 | Cashier | exits; total wait 51.7494, time in system 98.1257 | 1 | 0 |
| 5432.2674 | Seize | 341 | Cashier | pulled from queue after waiting 1.7889, service until 5435.9646 | 0 | 1 |
| 5435.9646 | Exit | 341 | Cashier | exits; total wait 74.1272, time in system 129.1619 | 0 | 0 |
| 5446.2262 | Arrival | 350 | Host | enters the system | 0 | 0 |
| 5446.2262 | Seize | 350 | Host | server free, service 1.7839 until 5448.0102 | 0 | 1 |
| 5448.0102 | Move | 350 | Host | service done, routing to Waiters | 0 | 0 |
| 5448.0102 | Queue | 350 | Waiters | all 3 busy, queued at position 5 | 5 | 3 |
| 5456.6809 | Move | 344 | Waiters | service done, routing to Cashier | 5 | 2 |
| 5456.6809 | Seize | 346 | Waiters | pulled from queue after waiting 63.9944, service until 5494.4749 | 4 | 3 |
| 5456.6809 | Seize | 344 | Cashier | server free, service 1.2125 until 5457.8934 | 0 | 1 |
| 5457.8934 | Exit | 344 | Cashier | exits; total wait 60.9088, time in system 88.9743 | 0 | 0 |
| 5458.6958 | Move | 343 | Waiters | service done, routing to Cashier | 4 | 2 |
| 5458.6958 | Seize | 347 | Waiters | pulled from queue after waiting 62.9103, service until 5501.5887 | 3 | 3 |
| 5458.6958 | Seize | 343 | Cashier | server free, service 2.6472 until 5461.3430 | 0 | 1 |
| 5461.3430 | Exit | 343 | Cashier | exits; total wait 50.9063, time in system 102.3315 | 0 | 0 |
| 5486.0994 | Arrival | 351 | Host | enters the system | 0 | 0 |
| 5486.0994 | Seize | 351 | Host | server free, service 0.2773 until 5486.3767 | 0 | 1 |
| 5486.3767 | Move | 351 | Host | service done, routing to Waiters | 0 | 0 |
| 5486.3767 | Queue | 351 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 5487.1226 | Move | 345 | Waiters | service done, routing to Cashier | 4 | 2 |
| 5487.1226 | Seize | 348 | Waiters | pulled from queue after waiting 86.7041, service until 5522.4258 | 3 | 3 |
| 5487.1226 | Seize | 345 | Cashier | server free, service 2.3087 until 5489.4313 | 0 | 1 |
| 5489.4313 | Exit | 345 | Cashier | exits; total wait 61.0040, time in system 120.4883 | 0 | 0 |
| 5494.4749 | Move | 346 | Waiters | service done, routing to Cashier | 3 | 2 |
| 5494.4749 | Seize | 349 | Waiters | pulled from queue after waiting 89.0124, service until 5529.8602 | 2 | 3 |
| 5494.4749 | Seize | 346 | Cashier | server free, service 1.1970 until 5495.6719 | 0 | 1 |
| 5495.6719 | Exit | 346 | Cashier | exits; total wait 63.9944, time in system 104.2137 | 0 | 0 |
| 5501.5887 | Move | 347 | Waiters | service done, routing to Cashier | 2 | 2 |
| 5501.5887 | Seize | 350 | Waiters | pulled from queue after waiting 53.5786, service until 5544.6645 | 1 | 3 |
| 5501.5887 | Seize | 347 | Cashier | server free, service 2.8308 until 5504.4195 | 0 | 1 |
| 5504.4195 | Exit | 347 | Cashier | exits; total wait 62.9103, time in system 111.6933 | 0 | 0 |
| 5522.4258 | Move | 348 | Waiters | service done, routing to Cashier | 1 | 2 |
| 5522.4258 | Seize | 351 | Waiters | pulled from queue after waiting 36.0491, service until 5563.0497 | 0 | 3 |
| 5522.4258 | Seize | 348 | Cashier | server free, service 1.2648 until 5523.6906 | 0 | 1 |
| 5523.6906 | Exit | 348 | Cashier | exits; total wait 88.5260, time in system 129.7269 | 0 | 0 |
| 5529.8602 | Move | 349 | Waiters | service done, routing to Cashier | 0 | 2 |
| 5529.8602 | Seize | 349 | Cashier | server free, service 1.7586 until 5531.6188 | 0 | 1 |
| 5531.6188 | Exit | 349 | Cashier | exits; total wait 89.3382, time in system 131.5261 | 0 | 0 |
| 5534.0839 | Arrival | 352 | Host | enters the system | 0 | 0 |
| 5534.0839 | Seize | 352 | Host | server free, service 0.1618 until 5534.2457 | 0 | 1 |
| 5534.2457 | Move | 352 | Host | service done, routing to Waiters | 0 | 0 |
| 5534.2457 | Seize | 352 | Waiters | server free, service 42.7004 until 5576.9461 | 0 | 3 |
| 5538.9140 | Arrival | 353 | Host | enters the system | 0 | 0 |
| 5538.9140 | Seize | 353 | Host | server free, service 1.5659 until 5540.4799 | 0 | 1 |
| 5540.4799 | Move | 353 | Host | service done, routing to Waiters | 0 | 0 |
| 5540.4799 | Queue | 353 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 5543.1048 | Arrival | 354 | Host | enters the system | 0 | 0 |
| 5543.1048 | Seize | 354 | Host | server free, service 0.4224 until 5543.5272 | 0 | 1 |
| 5543.5272 | Move | 354 | Host | service done, routing to Waiters | 0 | 0 |
| 5543.5272 | Queue | 354 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 5544.6645 | Move | 350 | Waiters | service done, routing to Cashier | 2 | 2 |
| 5544.6645 | Seize | 353 | Waiters | pulled from queue after waiting 4.1846, service until 5594.0287 | 1 | 3 |
| 5544.6645 | Seize | 350 | Cashier | server free, service 2.6545 until 5547.3190 | 0 | 1 |
| 5547.3190 | Exit | 350 | Cashier | exits; total wait 53.5786, time in system 101.0927 | 0 | 0 |
| 5562.5775 | Arrival | 355 | Host | enters the system | 0 | 0 |
| 5562.5775 | Seize | 355 | Host | server free, service 2.2457 until 5564.8232 | 0 | 1 |
| 5563.0497 | Move | 351 | Waiters | service done, routing to Cashier | 1 | 2 |
| 5563.0497 | Seize | 354 | Waiters | pulled from queue after waiting 19.5225, service until 5612.2687 | 0 | 3 |
| 5563.0497 | Seize | 351 | Cashier | server free, service 1.2448 until 5564.2945 | 0 | 1 |
| 5564.2945 | Exit | 351 | Cashier | exits; total wait 36.0491, time in system 78.1951 | 0 | 0 |
| 5564.8232 | Move | 355 | Host | service done, routing to Waiters | 0 | 0 |
| 5564.8232 | Queue | 355 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 5567.7087 | Arrival | 356 | Host | enters the system | 0 | 0 |
| 5567.7087 | Seize | 356 | Host | server free, service 0.2154 until 5567.9241 | 0 | 1 |
| 5567.9241 | Move | 356 | Host | service done, routing to Waiters | 0 | 0 |
| 5567.9241 | Queue | 356 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 5576.9461 | Move | 352 | Waiters | service done, routing to Cashier | 2 | 2 |
| 5576.9461 | Seize | 355 | Waiters | pulled from queue after waiting 12.1229, service until 5619.1359 | 1 | 3 |
| 5576.9461 | Seize | 352 | Cashier | server free, service 2.7472 until 5579.6932 | 0 | 1 |
| 5579.6932 | Exit | 352 | Cashier | exits; total wait 0.0000, time in system 45.6094 | 0 | 0 |
| 5594.0287 | Move | 353 | Waiters | service done, routing to Cashier | 1 | 2 |
| 5594.0287 | Seize | 356 | Waiters | pulled from queue after waiting 26.1046, service until 5645.4023 | 0 | 3 |
| 5594.0287 | Seize | 353 | Cashier | server free, service 2.6444 until 5596.6732 | 0 | 1 |
| 5596.6732 | Exit | 353 | Cashier | exits; total wait 4.1846, time in system 57.7592 | 0 | 0 |
| 5600.5147 | Arrival | 357 | Host | enters the system | 0 | 0 |
| 5600.5147 | Seize | 357 | Host | server free, service 1.4711 until 5601.9859 | 0 | 1 |
| 5601.9859 | Move | 357 | Host | service done, routing to Waiters | 0 | 0 |
| 5601.9859 | Queue | 357 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 5612.2687 | Move | 354 | Waiters | service done, routing to Cashier | 1 | 2 |
| 5612.2687 | Seize | 357 | Waiters | pulled from queue after waiting 10.2828, service until 5657.1451 | 0 | 3 |
| 5612.2687 | Seize | 354 | Cashier | server free, service 3.2499 until 5615.5186 | 0 | 1 |
| 5615.5186 | Exit | 354 | Cashier | exits; total wait 19.5225, time in system 72.4138 | 0 | 0 |
| 5619.1359 | Move | 355 | Waiters | service done, routing to Cashier | 0 | 2 |
| 5619.1359 | Seize | 355 | Cashier | server free, service 3.9857 until 5623.1216 | 0 | 1 |
| 5623.1216 | Exit | 355 | Cashier | exits; total wait 12.1229, time in system 60.5441 | 0 | 0 |
| 5633.1593 | Arrival | 358 | Host | enters the system | 0 | 0 |
| 5633.1593 | Seize | 358 | Host | server free, service 0.6123 until 5633.7717 | 0 | 1 |
| 5633.7717 | Move | 358 | Host | service done, routing to Waiters | 0 | 0 |
| 5633.7717 | Seize | 358 | Waiters | server free, service 36.6820 until 5670.4537 | 0 | 3 |
| 5636.2398 | Arrival | 359 | Host | enters the system | 0 | 0 |
| 5636.2398 | Seize | 359 | Host | server free, service 0.1218 until 5636.3616 | 0 | 1 |
| 5636.3616 | Move | 359 | Host | service done, routing to Waiters | 0 | 0 |
| 5636.3616 | Queue | 359 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 5645.4023 | Move | 356 | Waiters | service done, routing to Cashier | 1 | 2 |
| 5645.4023 | Seize | 359 | Waiters | pulled from queue after waiting 9.0407, service until 5679.4176 | 0 | 3 |
| 5645.4023 | Seize | 356 | Cashier | server free, service 3.5628 until 5648.9651 | 0 | 1 |
| 5648.9651 | Exit | 356 | Cashier | exits; total wait 26.1046, time in system 81.2564 | 0 | 0 |
| 5657.1451 | Move | 357 | Waiters | service done, routing to Cashier | 0 | 2 |
| 5657.1451 | Seize | 357 | Cashier | server free, service 2.4927 until 5659.6378 | 0 | 1 |
| 5657.4809 | Arrival | 360 | Host | enters the system | 0 | 0 |
| 5657.4809 | Seize | 360 | Host | server free, service 10.3698 until 5667.8507 | 0 | 1 |
| 5659.6378 | Exit | 357 | Cashier | exits; total wait 10.2828, time in system 59.1230 | 0 | 0 |
| 5665.3263 | Arrival | 361 | Host | enters the system | 0 | 1 |
| 5665.3263 | Queue | 361 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 5667.8507 | Move | 360 | Host | service done, routing to Waiters | 1 | 0 |
| 5667.8507 | Seize | 361 | Host | pulled from queue after waiting 2.5244, service until 5669.2663 | 0 | 1 |
| 5667.8507 | Seize | 360 | Waiters | server free, service 25.3946 until 5693.2453 | 0 | 3 |
| 5669.2663 | Move | 361 | Host | service done, routing to Waiters | 0 | 0 |
| 5669.2663 | Queue | 361 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 5670.4537 | Move | 358 | Waiters | service done, routing to Cashier | 1 | 2 |
| 5670.4537 | Seize | 361 | Waiters | pulled from queue after waiting 1.1874, service until 5704.5000 | 0 | 3 |
| 5670.4537 | Seize | 358 | Cashier | server free, service 3.7992 until 5674.2529 | 0 | 1 |
| 5674.2529 | Exit | 358 | Cashier | exits; total wait 0.0000, time in system 41.0936 | 0 | 0 |
| 5679.4176 | Move | 359 | Waiters | service done, routing to Cashier | 0 | 2 |
| 5679.4176 | Seize | 359 | Cashier | server free, service 1.8562 until 5681.2737 | 0 | 1 |
| 5681.2737 | Exit | 359 | Cashier | exits; total wait 9.0407, time in system 45.0339 | 0 | 0 |
| 5693.2453 | Move | 360 | Waiters | service done, routing to Cashier | 0 | 1 |
| 5693.2453 | Seize | 360 | Cashier | server free, service 3.4737 until 5696.7190 | 0 | 1 |
| 5696.7190 | Exit | 360 | Cashier | exits; total wait 0.0000, time in system 39.2381 | 0 | 0 |
| 5704.5000 | Move | 361 | Waiters | service done, routing to Cashier | 0 | 0 |
| 5704.5000 | Seize | 361 | Cashier | server free, service 2.1407 until 5706.6407 | 0 | 1 |
| 5706.6407 | Exit | 361 | Cashier | exits; total wait 3.7118, time in system 41.3144 | 0 | 0 |
| 5708.0415 | Arrival | 362 | Host | enters the system | 0 | 0 |
| 5708.0415 | Seize | 362 | Host | server free, service 0.8321 until 5708.8736 | 0 | 1 |
| 5708.8736 | Move | 362 | Host | service done, routing to Waiters | 0 | 0 |
| 5708.8736 | Seize | 362 | Waiters | server free, service 27.4070 until 5736.2806 | 0 | 1 |
| 5723.9090 | Arrival | 363 | Host | enters the system | 0 | 0 |
| 5723.9090 | Seize | 363 | Host | server free, service 2.2734 until 5726.1823 | 0 | 1 |
| 5726.0888 | Arrival | 364 | Host | enters the system | 0 | 1 |
| 5726.0888 | Queue | 364 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 5726.1823 | Move | 363 | Host | service done, routing to Waiters | 1 | 0 |
| 5726.1823 | Seize | 364 | Host | pulled from queue after waiting 0.0935, service until 5727.0745 | 0 | 1 |
| 5726.1823 | Seize | 363 | Waiters | server free, service 45.3714 until 5771.5537 | 0 | 2 |
| 5727.0745 | Move | 364 | Host | service done, routing to Waiters | 0 | 0 |
| 5727.0745 | Seize | 364 | Waiters | server free, service 38.0768 until 5765.1514 | 0 | 3 |
| 5734.5262 | Arrival | 365 | Host | enters the system | 0 | 0 |
| 5734.5262 | Seize | 365 | Host | server free, service 4.4123 until 5738.9385 | 0 | 1 |
| 5736.2806 | Move | 362 | Waiters | service done, routing to Cashier | 0 | 2 |
| 5736.2806 | Seize | 362 | Cashier | server free, service 2.1170 until 5738.3976 | 0 | 1 |
| 5738.3976 | Exit | 362 | Cashier | exits; total wait 0.0000, time in system 30.3561 | 0 | 0 |
| 5738.9385 | Move | 365 | Host | service done, routing to Waiters | 0 | 0 |
| 5738.9385 | Seize | 365 | Waiters | server free, service 41.8217 until 5780.7602 | 0 | 3 |
| 5743.9882 | Arrival | 366 | Host | enters the system | 0 | 0 |
| 5743.9882 | Seize | 366 | Host | server free, service 1.3282 until 5745.3164 | 0 | 1 |
| 5745.3164 | Move | 366 | Host | service done, routing to Waiters | 0 | 0 |
| 5745.3164 | Queue | 366 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 5745.5131 | Arrival | 367 | Host | enters the system | 0 | 0 |
| 5745.5131 | Seize | 367 | Host | server free, service 0.9250 until 5746.4381 | 0 | 1 |
| 5746.4381 | Move | 367 | Host | service done, routing to Waiters | 0 | 0 |
| 5746.4381 | Queue | 367 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 5752.6010 | Arrival | 368 | Host | enters the system | 0 | 0 |
| 5752.6010 | Seize | 368 | Host | server free, service 2.4091 until 5755.0102 | 0 | 1 |
| 5755.0102 | Move | 368 | Host | service done, routing to Waiters | 0 | 0 |
| 5755.0102 | Queue | 368 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 5765.1514 | Move | 364 | Waiters | service done, routing to Cashier | 3 | 2 |
| 5765.1514 | Seize | 366 | Waiters | pulled from queue after waiting 19.8350, service until 5791.3584 | 2 | 3 |
| 5765.1514 | Seize | 364 | Cashier | server free, service 3.5219 until 5768.6733 | 0 | 1 |
| 5768.6733 | Exit | 364 | Cashier | exits; total wait 0.0935, time in system 42.5844 | 0 | 0 |
| 5771.5537 | Move | 363 | Waiters | service done, routing to Cashier | 2 | 2 |
| 5771.5537 | Seize | 367 | Waiters | pulled from queue after waiting 25.1156, service until 5819.6904 | 1 | 3 |
| 5771.5537 | Seize | 363 | Cashier | server free, service 1.2398 until 5772.7935 | 0 | 1 |
| 5772.7935 | Exit | 363 | Cashier | exits; total wait 0.0000, time in system 48.8846 | 0 | 0 |
| 5780.7602 | Move | 365 | Waiters | service done, routing to Cashier | 1 | 2 |
| 5780.7602 | Seize | 368 | Waiters | pulled from queue after waiting 25.7500, service until 5816.3415 | 0 | 3 |
| 5780.7602 | Seize | 365 | Cashier | server free, service 1.4750 until 5782.2352 | 0 | 1 |
| 5782.2352 | Exit | 365 | Cashier | exits; total wait 0.0000, time in system 47.7090 | 0 | 0 |
| 5791.3584 | Move | 366 | Waiters | service done, routing to Cashier | 0 | 2 |
| 5791.3584 | Seize | 366 | Cashier | server free, service 2.0364 until 5793.3948 | 0 | 1 |
| 5793.3948 | Exit | 366 | Cashier | exits; total wait 19.8350, time in system 49.4065 | 0 | 0 |
| 5794.2449 | Arrival | 369 | Host | enters the system | 0 | 0 |
| 5794.2449 | Seize | 369 | Host | server free, service 1.9118 until 5796.1568 | 0 | 1 |
| 5794.2546 | Arrival | 370 | Host | enters the system | 0 | 1 |
| 5794.2546 | Queue | 370 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 5796.1568 | Move | 369 | Host | service done, routing to Waiters | 1 | 0 |
| 5796.1568 | Seize | 370 | Host | pulled from queue after waiting 1.9021, service until 5799.5953 | 0 | 1 |
| 5796.1568 | Seize | 369 | Waiters | server free, service 53.2466 until 5849.4033 | 0 | 3 |
| 5797.7993 | Arrival | 371 | Host | enters the system | 0 | 1 |
| 5797.7993 | Queue | 371 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 5799.5953 | Move | 370 | Host | service done, routing to Waiters | 1 | 0 |
| 5799.5953 | Seize | 371 | Host | pulled from queue after waiting 1.7960, service until 5805.4340 | 0 | 1 |
| 5799.5953 | Queue | 370 | Waiters | all 3 busy, queued at position 1 | 1 | 3 |
| 5802.0443 | Arrival | 372 | Host | enters the system | 0 | 1 |
| 5802.0443 | Queue | 372 | Host | all 1 busy, queued at position 1 | 1 | 1 |
| 5805.4340 | Move | 371 | Host | service done, routing to Waiters | 1 | 0 |
| 5805.4340 | Seize | 372 | Host | pulled from queue after waiting 3.3897, service until 5808.5737 | 0 | 1 |
| 5805.4340 | Queue | 371 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 5808.5737 | Move | 372 | Host | service done, routing to Waiters | 0 | 0 |
| 5808.5737 | Queue | 372 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 5816.3415 | Move | 368 | Waiters | service done, routing to Cashier | 3 | 2 |
| 5816.3415 | Seize | 370 | Waiters | pulled from queue after waiting 16.7461, service until 5857.0387 | 2 | 3 |
| 5816.3415 | Seize | 368 | Cashier | server free, service 3.0696 until 5819.4111 | 0 | 1 |
| 5819.4111 | Exit | 368 | Cashier | exits; total wait 25.7500, time in system 66.8100 | 0 | 0 |
| 5819.6904 | Move | 367 | Waiters | service done, routing to Cashier | 2 | 2 |
| 5819.6904 | Seize | 371 | Waiters | pulled from queue after waiting 14.2564, service until 5859.8285 | 1 | 3 |
| 5819.6904 | Seize | 367 | Cashier | server free, service 3.0787 until 5822.7691 | 0 | 1 |
| 5820.4593 | Arrival | 373 | Host | enters the system | 0 | 0 |
| 5820.4593 | Seize | 373 | Host | server free, service 1.6684 until 5822.1278 | 0 | 1 |
| 5822.1278 | Move | 373 | Host | service done, routing to Waiters | 0 | 0 |
| 5822.1278 | Queue | 373 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 5822.7691 | Exit | 367 | Cashier | exits; total wait 25.1156, time in system 77.2560 | 0 | 0 |
| 5823.4255 | Arrival | 374 | Host | enters the system | 0 | 0 |
| 5823.4255 | Seize | 374 | Host | server free, service 0.0156 until 5823.4411 | 0 | 1 |
| 5823.4411 | Move | 374 | Host | service done, routing to Waiters | 0 | 0 |
| 5823.4411 | Queue | 374 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 5831.2937 | Arrival | 375 | Host | enters the system | 0 | 0 |
| 5831.2937 | Seize | 375 | Host | server free, service 3.3635 until 5834.6572 | 0 | 1 |
| 5834.6572 | Move | 375 | Host | service done, routing to Waiters | 0 | 0 |
| 5834.6572 | Queue | 375 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 5843.4384 | Arrival | 376 | Host | enters the system | 0 | 0 |
| 5843.4384 | Seize | 376 | Host | server free, service 0.8357 until 5844.2741 | 0 | 1 |
| 5844.2741 | Move | 376 | Host | service done, routing to Waiters | 0 | 0 |
| 5844.2741 | Queue | 376 | Waiters | all 3 busy, queued at position 5 | 5 | 3 |
| 5844.5030 | Arrival | 377 | Host | enters the system | 0 | 0 |
| 5844.5030 | Seize | 377 | Host | server free, service 0.3804 until 5844.8834 | 0 | 1 |
| 5844.8834 | Move | 377 | Host | service done, routing to Waiters | 0 | 0 |
| 5844.8834 | Queue | 377 | Waiters | all 3 busy, queued at position 6 | 6 | 3 |
| 5849.4033 | Move | 369 | Waiters | service done, routing to Cashier | 6 | 2 |
| 5849.4033 | Seize | 372 | Waiters | pulled from queue after waiting 40.8296, service until 5890.9743 | 5 | 3 |
| 5849.4033 | Seize | 369 | Cashier | server free, service 3.3376 until 5852.7410 | 0 | 1 |
| 5852.7410 | Exit | 369 | Cashier | exits; total wait 0.0000, time in system 58.4960 | 0 | 0 |
| 5857.0387 | Move | 370 | Waiters | service done, routing to Cashier | 5 | 2 |
| 5857.0387 | Seize | 373 | Waiters | pulled from queue after waiting 34.9109, service until 5891.3363 | 4 | 3 |
| 5857.0387 | Seize | 370 | Cashier | server free, service 1.2889 until 5858.3275 | 0 | 1 |
| 5858.3275 | Exit | 370 | Cashier | exits; total wait 18.6482, time in system 64.0729 | 0 | 0 |
| 5859.8285 | Move | 371 | Waiters | service done, routing to Cashier | 4 | 2 |
| 5859.8285 | Seize | 374 | Waiters | pulled from queue after waiting 36.3874, service until 5888.5285 | 3 | 3 |
| 5859.8285 | Seize | 371 | Cashier | server free, service 1.4949 until 5861.3234 | 0 | 1 |
| 5861.3234 | Exit | 371 | Cashier | exits; total wait 16.0524, time in system 63.5241 | 0 | 0 |
| 5869.7102 | Arrival | 378 | Host | enters the system | 0 | 0 |
| 5869.7102 | Seize | 378 | Host | server free, service 13.6776 until 5883.3878 | 0 | 1 |
| 5883.3878 | Move | 378 | Host | service done, routing to Waiters | 0 | 0 |
| 5883.3878 | Queue | 378 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 5887.6851 | Arrival | 379 | Host | enters the system | 0 | 0 |
| 5887.6851 | Seize | 379 | Host | server free, service 2.1120 until 5889.7971 | 0 | 1 |
| 5888.5285 | Move | 374 | Waiters | service done, routing to Cashier | 4 | 2 |
| 5888.5285 | Seize | 375 | Waiters | pulled from queue after waiting 53.8712, service until 5926.1290 | 3 | 3 |
| 5888.5285 | Seize | 374 | Cashier | server free, service 3.6465 until 5892.1749 | 0 | 1 |
| 5889.7971 | Move | 379 | Host | service done, routing to Waiters | 0 | 0 |
| 5889.7971 | Queue | 379 | Waiters | all 3 busy, queued at position 4 | 4 | 3 |
| 5890.9743 | Move | 372 | Waiters | service done, routing to Cashier | 4 | 2 |
| 5890.9743 | Seize | 376 | Waiters | pulled from queue after waiting 46.7002, service until 5938.4344 | 3 | 3 |
| 5890.9743 | Queue | 372 | Cashier | all 1 busy, queued at position 1 | 1 | 1 |
| 5891.3363 | Move | 373 | Waiters | service done, routing to Cashier | 3 | 2 |
| 5891.3363 | Seize | 377 | Waiters | pulled from queue after waiting 46.4529, service until 5928.5379 | 2 | 3 |
| 5891.3363 | Queue | 373 | Cashier | all 1 busy, queued at position 2 | 2 | 1 |
| 5892.1749 | Exit | 374 | Cashier | exits; total wait 36.3874, time in system 68.7494 | 2 | 0 |
| 5892.1749 | Seize | 372 | Cashier | pulled from queue after waiting 1.2006, service until 5895.5918 | 1 | 1 |
| 5895.5918 | Exit | 372 | Cashier | exits; total wait 45.4200, time in system 93.5475 | 1 | 0 |
| 5895.5918 | Seize | 373 | Cashier | pulled from queue after waiting 4.2556, service until 5898.9623 | 0 | 1 |
| 5898.9623 | Exit | 373 | Cashier | exits; total wait 39.1665, time in system 78.5030 | 0 | 0 |
| 5913.2690 | Arrival | 380 | Host | enters the system | 0 | 0 |
| 5913.2690 | Seize | 380 | Host | server free, service 0.3032 until 5913.5722 | 0 | 1 |
| 5913.5722 | Move | 380 | Host | service done, routing to Waiters | 0 | 0 |
| 5913.5722 | Queue | 380 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 5926.1290 | Move | 375 | Waiters | service done, routing to Cashier | 3 | 2 |
| 5926.1290 | Seize | 378 | Waiters | pulled from queue after waiting 42.7412, service until 5957.9889 | 2 | 3 |
| 5926.1290 | Seize | 375 | Cashier | server free, service 1.8375 until 5927.9665 | 0 | 1 |
| 5927.9665 | Exit | 375 | Cashier | exits; total wait 53.8712, time in system 96.6728 | 0 | 0 |
| 5928.5379 | Move | 377 | Waiters | service done, routing to Cashier | 2 | 2 |
| 5928.5379 | Seize | 379 | Waiters | pulled from queue after waiting 38.7408, service until 5979.9304 | 1 | 3 |
| 5928.5379 | Seize | 377 | Cashier | server free, service 3.4563 until 5931.9942 | 0 | 1 |
| 5929.2012 | Arrival | 381 | Host | enters the system | 0 | 0 |
| 5929.2012 | Seize | 381 | Host | server free, service 1.3384 until 5930.5396 | 0 | 1 |
| 5930.5396 | Move | 381 | Host | service done, routing to Waiters | 0 | 0 |
| 5930.5396 | Queue | 381 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 5931.9942 | Exit | 377 | Cashier | exits; total wait 46.4529, time in system 87.4912 | 0 | 0 |
| 5938.4344 | Move | 376 | Waiters | service done, routing to Cashier | 2 | 2 |
| 5938.4344 | Seize | 380 | Waiters | pulled from queue after waiting 24.8622, service until 5976.8409 | 1 | 3 |
| 5938.4344 | Seize | 376 | Cashier | server free, service 1.1662 until 5939.6007 | 0 | 1 |
| 5939.6007 | Exit | 376 | Cashier | exits; total wait 46.7002, time in system 96.1623 | 0 | 0 |
| 5946.5425 | Arrival | 382 | Host | enters the system | 0 | 0 |
| 5946.5425 | Seize | 382 | Host | server free, service 0.1708 until 5946.7134 | 0 | 1 |
| 5946.7134 | Move | 382 | Host | service done, routing to Waiters | 0 | 0 |
| 5946.7134 | Queue | 382 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 5957.9889 | Move | 378 | Waiters | service done, routing to Cashier | 2 | 2 |
| 5957.9889 | Seize | 381 | Waiters | pulled from queue after waiting 27.4493, service until 5996.6820 | 1 | 3 |
| 5957.9889 | Seize | 378 | Cashier | server free, service 2.4151 until 5960.4039 | 0 | 1 |
| 5958.2018 | Arrival | 383 | Host | enters the system | 0 | 0 |
| 5958.2018 | Seize | 383 | Host | server free, service 0.3727 until 5958.5745 | 0 | 1 |
| 5958.5745 | Move | 383 | Host | service done, routing to Waiters | 0 | 0 |
| 5958.5745 | Queue | 383 | Waiters | all 3 busy, queued at position 2 | 2 | 3 |
| 5960.4039 | Exit | 378 | Cashier | exits; total wait 42.7412, time in system 90.6937 | 0 | 0 |
| 5973.8590 | Arrival | 384 | Host | enters the system | 0 | 0 |
| 5973.8590 | Seize | 384 | Host | server free, service 0.0571 until 5973.9161 | 0 | 1 |
| 5973.9161 | Move | 384 | Host | service done, routing to Waiters | 0 | 0 |
| 5973.9161 | Queue | 384 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 5976.8409 | Move | 380 | Waiters | service done, routing to Cashier | 3 | 2 |
| 5976.8409 | Seize | 382 | Waiters | pulled from queue after waiting 30.1276, service until 6034.1760 | 2 | 3 |
| 5976.8409 | Seize | 380 | Cashier | server free, service 2.8434 until 5979.6844 | 0 | 1 |
| 5976.8921 | Arrival | 385 | Host | enters the system | 0 | 0 |
| 5976.8921 | Seize | 385 | Host | server free, service 1.2041 until 5978.0962 | 0 | 1 |
| 5978.0962 | Move | 385 | Host | service done, routing to Waiters | 0 | 0 |
| 5978.0962 | Queue | 385 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 5979.6844 | Exit | 380 | Cashier | exits; total wait 24.8622, time in system 66.4154 | 0 | 0 |
| 5979.9304 | Move | 379 | Waiters | service done, routing to Cashier | 3 | 2 |
| 5979.9304 | Seize | 383 | Waiters | pulled from queue after waiting 21.3559, service until 6012.6968 | 2 | 3 |
| 5979.9304 | Seize | 379 | Cashier | server free, service 1.9978 until 5981.9282 | 0 | 1 |
| 5981.9282 | Exit | 379 | Cashier | exits; total wait 38.7408, time in system 94.2431 | 0 | 0 |
| 5982.5912 | Arrival | 386 | Host | enters the system | 0 | 0 |
| 5982.5912 | Seize | 386 | Host | server free, service 1.4224 until 5984.0136 | 0 | 1 |
| 5984.0136 | Move | 386 | Host | service done, routing to Waiters | 0 | 0 |
| 5984.0136 | Queue | 386 | Waiters | all 3 busy, queued at position 3 | 3 | 3 |
| 5996.6820 | Move | 381 | Waiters | service done, routing to Cashier | 3 | 2 |
| 5996.6820 | Seize | 384 | Waiters | pulled from queue after waiting 22.7659, service until 6033.5377 | 2 | 3 |
| 5996.6820 | Seize | 381 | Cashier | server free, service 1.8644 until 5998.5464 | 0 | 1 |
| 5998.5464 | Exit | 381 | Cashier | exits; total wait 27.4493, time in system 69.3452 | 0 | 0 |
| 6012.6968 | Move | 383 | Waiters | service done, routing to Cashier | 2 | 2 |
| 6012.6968 | Seize | 385 | Waiters | pulled from queue after waiting 34.6006, service until 6054.0318 | 1 | 3 |
| 6012.6968 | Seize | 383 | Cashier | server free, service 2.0155 until 6014.7123 | 0 | 1 |

*3027 events traced.*
