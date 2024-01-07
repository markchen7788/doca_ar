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