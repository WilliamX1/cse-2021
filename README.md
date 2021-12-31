# cse-2021-fall

很难想象，在大三上学期我上到了一门特别有趣，看似科普实际上却干货满满的课程 ~ 计算机系统工程。软件学院夏虞斌老师为这门课赋予了灵魂，而这四个 lab 则加深了我对计算机系统的了解。

`注：每个 lab 可以从不同分支处找到，每个 lab 都是基于前一个 lab 进行实现的。`

**Lab 1: Basic File System**

在 lab 1 中，我完成了一个 File System 的雏形。

PART1 中完成了 CREATE/GETATTR/PUT/GET/REMOVE 这五个系统 API。

PART2 中完成了 CREATE/MKNOD/LOOKUP/READDIR/SETATTR/WRITE/READ/MKDIR/UNLINK/SIMBOLIC LINK 这几个基于底层的拓展 API。

参考文档在 [**这里**](https://ipads.se.sjtu.edu.cn/courses/cse/labs/lab1.html)，感谢 **沈嘉欢** 学长负责这个 lab 并为我提供了许多帮助。

**Lab 2: Word Count with MapReduce**

在 lab2 中，我完成了一个 MapReduce 的工具，通过 RPC 调用实现了分布式系统上的单词数量统计。

PART1 中完成了一个单机单线程的 MapReduce 工具。

PART2 中完成了一个分布式多线程的 MapReduce 统计。

参考文档在 [**这里**](https://ipads.se.sjtu.edu.cn/courses/cse/labs/lab2.html)，感谢 **李明煜** 学长负责这个 lab 并为我提供了许多帮助。

**Lab 3: Fault-tolerant Key-Value Store with Raft**

在 lab3 中，我完成了一个 Raft 的 Key-Value 存储，基于 [Raft 论文](https://ipads.se.sjtu.edu.cn/courses/cse/labs/lab3-assets/raft.pdf)。

PART1 中完成了 Leader 选举。

PART2 中完成了日志备份。

PART3 中完成了日志的持久化存储。

PART4 中完成了快照存储。

PART5 中完成了一个可容错的键值对存储。

参考文档在 [**这里**](https://ipads.se.sjtu.edu.cn/courses/cse/labs/lab3.html)，感谢 **韩明聪** 学长负责这个 lab 并为我提供了许多帮助。

**Lab 4: Shard Transactional KVS Service**

在 lab4 中，我完成了一个分片的支持事务的 Key-Value 存储服务。

PART1 中完成了将键值对分区到多个分片客户端，并且完成了 Two Phase Commit 事务支持。

PART2 中加入了 RAFT 元素。

PART3 中完成了锁相关的操作。

参考文档在 [**这里**](https://ipads.se.sjtu.edu.cn/courses/cse/labs/lab4.html)，感谢 **陆放明** 学长负责这个 lab 并为我提供了许多帮助。

