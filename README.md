![doca-ar-logo.png](./docs/logo.png)
# DOCA-AR

#### 介绍
* **应用简介：基于DOCA的自适应路由**
    1. 利用NVIDIA BlueField-2 DPU卸载基于主动探测的自适应路由算法，实现VXLAN等overlay流量的逐流负载均衡;
    2. DOCA-AR实现了基于全局拥塞感知的负载均衡，通过在端侧发送探测报文获取拥塞状态并帮助流量避开拥塞点从而改善尾时延；
    3. DOCA-AR部署在DPU，既拥有主机方案易感知全局状态的特点，也拥有交换机方案不修改主机协议栈的优势。
* **性能表现：如下是在我们的多路径网络下测试的结果**
    1. 第一张表，测试了10条5MB大小的流的最大完成时间（MaxFCT:尾时延）；
    2. 第二张表，测试了50条5MB大小的流的最大完成时间（MaxFCT:尾时延）；
    3. “With an 40Gbps Elephant Flow”代表引入一条40Gbps的大象流致使一条路径产生拥塞；
    4. 可以看出相对于ECMP，DOCA-AR能够很好的避开拥塞点，改善尾时延，详细测试说明可参看后续内容；

<table >
    <thead>
    <tr>
        <th>FlowNum</td>
        <th>MessageSize</td>
        <th colspan=3>Test Times</td>
    </tr>
    </thead>
    <tr align="center">
        <td>10</td>
        <td>5MB</td>
        <td colspan=3>30 times</td>
    </tr>
    <thead>
    <tr>
        <th>Network Load</td>
        <th>Load Balancing Scheme</td>
        <th>MaxFCT-Min[ms]</td>
        <th>MaxFCT-Max[ms]</td>
        <th>MaxFCT-Avg[ms]</td>
    </tr>
    <thead>
    <tr align="center">
        <td rowspan=2>With an 40Gbps Elephant Flow</td>
        <td>DOCA-AR</td>
        <td>22.74</td>
        <td>231.29</td>
        <td>34.21</td>
    </tr>
    <tr align="center">
        <td>ECMP</td>
        <td>27.41</td>
        <td>1709.99</td>
        <td>920.81</td>
    </tr>
    <tr align="center">
        <td rowspan=2>No Elephant Flow</td>
        <td>DOCA-AR</td>
        <td>23.54</td>
        <td>34.49</td>
        <td>27.32</td>
    </tr>
    <tr align="center">
        <td>ECMP</td>
        <td>20.42</td>
        <td>35.1</td>
        <td>26.97</td>
    </tr>
</table>

<table >
    <thead>
    <tr>
        <th>FlowNum</td>
        <th>MessageSize</td>
        <th colspan=3>Test Times</td>
    </tr>
    </thead>
    <tr align="center">
        <td>50</td>
        <td>5MB</td>
        <td colspan=3>30 times</td>
    </tr>
    <thead>
    <tr>
        <th>Network Load</td>
        <th>Load Balancing Scheme</td>
        <th>MaxFCT-Min[ms]</td>
        <th>MaxFCT-Max[ms]</td>
        <th>MaxFCT-Avg[ms]</td>
    </tr>
    <thead>
    <tr align="center">
        <td rowspan=2>With an 40Gbps Elephant Flow</td>
        <td>DOCA-AR</td>
        <td>103.45</td>
        <td>1535.76</td>
        <td>334.52</td>
    </tr>
    <tr align="center">
        <td>ECMP</td>
        <td>1127.96</td>
        <td>2235.71</td>
        <td>1857.05</td>
    </tr>
    <tr align="center">
        <td rowspan=2>No Elephant Flow</td>
        <td>DOCA-AR</td>
        <td>98.35</td>
        <td>292.31</td>
        <td>221.72</td>
    </tr>
    <tr align="center">
        <td>ECMP</td>
        <td>100.83</td>
        <td>289.77</td>
        <td>236.18</td>
    </tr>
</table>


* **使用背景**：
    * 云计算和AI等技术的蓬勃发展，数据中心东西向流量逐渐增多，带宽跃升至100Gbps甚至400Gbps；
    * 数据中心网络通常是Spine-Leaf等包含冗余链路的多路径架构，服务器之间的流量通常会被路由到不同路径来实现网络负载均衡，从而提高链路利用率、提升吞吐并改善流量的传输完成时间(FCT)；
    * 数据中心的并发场景下，通常大家会更关注长尾时延（类似木桶效应，最坏的情况决定整体表现），网络负载越均衡尾时延通常也会越小；
    * DCN(数据中心网络)负载均衡一直是热议话题，几乎每年的网络顶会都会出现相关文章，我们将它们划分成了不同类型：
        * 按照是否感知拥塞状态分类：
            * 感知拥塞，例如CONGA，能避开拥塞点但开销大；
            * 不感知拥塞，例如ECMP，开销小但不能避开拥塞点；
        * 按照调度粒度分类：
            * 逐流，例如ECMP，PLB，开销最小但是粒度大；
            * 逐flowlet，例如CONGA；
            * 逐包，例如NVIDIA spectrum自带的Adaptive Routing，乱序导致开销大但是粒度细；
        * 按照实现位置分类：
            * 基于交换机，例如NVIDIA Spectrum自带的Adaptive Routing，不易感知全局拥塞状态、实现比端侧难但不需要端侧修改协议栈；
            * 基于Host，例如CLOVE，容易感知全局拥塞状态但通常需要修改主机协议栈；
            * 基于控制器，例如Fastpass；
    * 目前数据中心比较常用的网络层负载均衡机制是ECMP，它采用逐流、无状态的调度，不感知本地的拥塞状态也不感知全局的拥塞状态；
* **应用价值**：
    * **探索了基于DPU的新型负载均衡方案的可行性和有效性，并提供了有效的源代码和测试数据作为支持；**
    * **相对于ECMP，极大得改善了长尾时延问题，部署实现简单、不需要修改协议栈且节省调度带来的主机CPU开销。**

#### 软件架构
软件架构说明


#### 使用教程

1.  xxxx
2.  xxxx
3.  xxxx

#### 测试说明
软件架构说明


#### 演示及文档
* 演示视频已上传至百度网盘，点我可前往查看实验过程；
* 点我可查看API文档