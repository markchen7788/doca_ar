<table>
   <thead>
    <tr>
        <td>FlowNum</td>
        <td>MessageSize</td>
        <td colspan=3>Test Times</td>
    </tr>
    <thead>
        <>
    <tr>
        <td>10</td>
        <td>5MB</td>
        <td colspan=3>30 times</td>
    </tr>
    <tr>
        <td>Network Load</td>
        <td>Load Balancing Scheme</td>
        <td>MaxFCT-Min[ms]</td>
        <td>MaxFCT-Max[ms]</td>
        <td>MaxFCT-Avg[ms]</td>
    </tr>
    <tr>
        <td rowspan=2>With an 40Gbps Elephant Flow</td>
        <td>DOCA-AR</td>
        <td>22.74</td>
        <td>231.29</td>
        <td>34.21</td>
    </tr>
    <tr>
        <td>ECMP</td>
        <td>27.41</td>
        <td>1709.99</td>
        <td>920.81</td>
    </tr>
    <tr>
        <td rowspan=2>No Elephant Flow</td>
        <td>DOCA-AR</td>
        <td>23.54</td>
        <td>34.49</td>
        <td>27.32</td>
    </tr>
    <tr>
        <td>ECMP</td>
        <td>20.42</td>
        <td>35.1</td>
        <td>26.97</td>
    </tr>
</table>