# tcp-mss
Tool that shows TCP MSS between two instances

# Compile
Nothing special needed.
```
$ gcc -o tcp-mss tcp-mss.c
```

# Usage
1. Launch server:
```
$ ./tcp-mss -s
```
2. Launch client:
```
$ ./tcp-mss -c <IP address of server>
```
