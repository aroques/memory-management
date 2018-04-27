## Memory Management

**OSS (operating system similator)** is a program that simulates an operating system.

This program simulates how a Operating System mangages memory.

The second chance page replacement algorithm is used.

To build this program run:
```
make
```

To run this program:
```    
./oss [-n number of max concurrent children processes]
```
The -n flag is optional. The default number of max concurrent children processes is 8.

To cleanup run:
```
make clean
```

#### Below is my git log:
2fb547a 2018-04-26 22:31:17 -0500 Update README.md  
78a91d6 2018-04-26 22:26:02 -0500 Add logging of memory map  
bd6e1c7 2018-04-26 21:12:05 -0500 Add extra time for swapping dirty page  
75c5994 2018-04-26 15:04:11 -0500 Stub print main memory function  
ce82127 2018-04-26 14:59:45 -0500 Implement second chance algorithm  
3e2565f 2018-04-26 11:47:30 -0500 Refactor oss for readability  
f12c84d 2018-04-26 11:45:14 -0500 Add comments for readability  
b3d1719 2018-04-26 10:26:07 -0500 Add/polish statistics  
60bdad7 2018-04-26 09:55:59 -0500 Debug oss main loop  
f5d8532 2018-04-26 09:21:26 -0500 Improve logging  
1233827 2018-04-26 08:58:33 -0500 Add response logic to oss  
9edf8e4 2018-04-25 05:59:41 -0500 Continue adding oss response logic  
bac53e0 2018-04-24 15:05:08 -0500 Add new blocked info struct to oss  
6ca30f3 2018-04-24 14:31:10 -0500 Add response logic and more logging to oss  
da11719 2018-04-24 11:28:04 -0500 Refactor oss response logic  
2be5ef9 2018-04-24 10:27:05 -0500 Add response logic to OSS  
8f16298 2018-04-23 20:24:34 -0500 Add more response logic in OSS  
457010e 2018-04-23 19:25:26 -0500 Add response logic to oss  
a31fbee 2018-04-22 19:02:12 -0500 Initial commit  
