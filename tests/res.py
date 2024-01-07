import os
import numpy as np
import matplotlib.pyplot as plt
files = []
for i, j, k in os.walk("."):
    # print(i)
    for file in k:
        if "txt" in file:
            files.append(i+"/"+file.replace(".txt", ""))



for file in files:
    data = []
    with open(file+".txt") as temp_f:
        datafile = temp_f.readlines()
    for line in datafile:
        cps = line.split()
        # print(cps)
        if cps[7].find("Mb")>=0:
            bits=float(cps[4])*1024*1024*8
            bps=float(cps[6])*1000
            # print(bps," ",bits," ",bits/bps)
            data.append(bits/bps)
        else:
            bits=float(cps[4])*1024*1024*8
            bps=float(cps[6])*1000000
            data.append(bits/bps)
    
    print(file,"\n(min[ms],max[ms],avg[ms](",round(min(data),2),round(max(data),2),round(sum(data)/len(data),2),")")

# print(data)
        
