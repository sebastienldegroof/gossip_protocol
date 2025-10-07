
# SWIM-style Membership/Gossip Protocol
## Description

Based on the MP1 assignment in the CS425 Distributed Systems course taught by the brilliant Professor Indy Gupta at the Univeristy of Illinois Urbana-Champaign.

## How to build the application

In your CLI:
```
make
```

## Local testing suite

```
./Tester.sh
```

## Individual tests

```
./Application ./testcases/singlefailure.conf
./Application ./testcases/multifailure.conf
./Application ./testcases/msgdropsinglefailure.conf
```

You may need to do `make clean && make` in between tests to make sure you have a clean run.
