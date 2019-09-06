# Spatial-SWIM
This is a NS-3 simlation of Medley protocol
 
How to run a Simulation
-------------------
```
./waf --run "simulate-energy"  
```
Ther are multiple options could be set to simulate different failure cases and topology setting.
- Option `topo_type` sets the topology used. The value could be zero to two, zero by default. 0 - grid, 1 - random, 2 - cluster
- Option `power_k` represents the exponential parameter to localize pinging. The larger `power_k` is, the more localized the pings will be. `power_k` is by default 3.
- Option `n_direct_ping` sets the number of direct pings that each node sends in each time period, by default 1.
- Option `n_ind_ping` sets the number of helpers that each sender needs for help with indirect ping.
- Option `num_failure` sets the number of simultaneous fail-stop nodes, by default 1.
- Option `t_failure` sets the time that the failures happen, by default 1500.
- Option `t_max` sets the system termination time, by default 3000.
- Option `fail_mode` simulates the fail-stop pattern. The mode could be one of the following:
    - Fixed: for grid and random, randomly choose several nodes to fail
    - Intra: for cluster mode only, the simultaneous failures happen within one of the clusters
    - Inter: for cluster mode only, the simultaneous failures happen among two neighbor clusters and the failed nodes are relatively close to each other.
    - Separate: for cluster mode only, the failed nodes are spread in different clusters.
- Option `input_mode` is the postfix of topology-related files' name, formatted as topo_type+number of nodes in network, e.g. random_25.
- Option `packet_loss` sets the packet loss rate (percentage) in network 0 - 100. By default is 0, which is reliable network with no packet loss.
- Option `timeout_multi` sets how many times the suspecion timeout is of period length. By default is -1, which will be set as 3 * ceil(log(N+1)) during system initialization, where N is the number of the nodes in the network.
- Option `r_tmp_outage` sets the temporary outage rate of each nodes, by default 0.0.
- Option `t_tmp_outage` sets the maximal time of a temporary outage, by default 100 time unit.
 
Running command example
```
./waf --run "simulate-energy --topo_type=cluster --power_k=3.0 --fail_mode=domain --input_mode=cluster_25 --num_failure=6"
```

