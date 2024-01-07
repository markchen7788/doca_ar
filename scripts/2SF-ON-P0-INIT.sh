ovs-vsctl del-br sf4
ovs-vsctl del-br sf5
ovs-vsctl add-br sf4
ovs-vsctl add-br sf5
ovs-vsctl del-port pf0hpf
ovs-vsctl del-port p0
ovs-vsctl del-port en3f0pf0sf4
ovs-vsctl del-port en3f0pf0sf5
ovs-vsctl add-port sf4 pf0hpf
ovs-vsctl add-port sf5 p0
ovs-vsctl add-port sf4 en3f0pf0sf4
ovs-vsctl add-port sf5 en3f0pf0sf5

ifconfig enp3s0f0s4 up
ifconfig enp3s0f0s5 up

#sf flow tables offload
sudo  ethtool -K pf0hpf hw-tc-offload on
sudo  ethtool -K p0 hw-tc-offload on
sudo  ethtool -K enp3s0f0s4 hw-tc-offload on
sudo  ethtool -K enp3s0f0s5 hw-tc-offload on
sudo  ethtool -K en3f0pf0sf4 hw-tc-offload on
sudo  ethtool -K en3f0pf0sf5 hw-tc-offload on
sudo  ovs-appctl dpctl/dump-flows type=offloaded
sudo  ovs-vsctl set Open_vSwitch . Other_config:hw-offload=true
sudo  /etc/init.d/openvswitch-switch restart

sudo ovs-ofctl del-flows sf5
sudo ovs-ofctl add-flow sf5 "in_port=p0 action=en3f0pf0sf5"
sudo ovs-ofctl add-flow sf5 "in_port=en3f0pf0sf5 action=p0"

sudo ovs-ofctl del-flows sf4
sudo ovs-ofctl add-flow sf4 "in_port=pf0hpf action=en3f0pf0sf4"
sudo ovs-ofctl add-flow sf4 "in_port=en3f0pf0sf4 action=pf0hpf"
