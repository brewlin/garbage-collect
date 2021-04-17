
## gc    trigger time
1. [ ]  mallocgc() 中本地缓存不够用的时候，会向mcentral中心申请，这种会触发gc
2. [ ]  定时最多20s 会触发一次gc

## sweep trigger time

1. [ ] 程序启动时，main阶段会创建一个死循环协程`bgsweep` 一直执行清除、监控任务
2. [ ] 当用户调用runtime.GC()时，会阻塞gc行为，导致清除阶段也会阻塞进行
3. [ ] gcStart() 时会去清除上一次gc残留的 span
4. [ ] 惰性清除。在向中央mcentral申请span时会默认进行 sweep
5. [ ] 标记结束阶段，会执行抢占所有线程，然后在安全点进行清除每个p的本地mcache


## stw 
1. [ ] 在进入标记阶段 gcoff -> gcmark 时需要stw
2. [ ] 在标记结束后   gcmark -> gcmarktermination 后需要 stw