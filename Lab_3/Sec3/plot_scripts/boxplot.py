# Read the documentation:
# https://matplotlib.org/3.1.1/api/_as_gen/matplotlib.pyplot.boxplot.html

# Note that we do not need to always use the seaborn library
import numpy as np
import matplotlib.pyplot as plt

fig1, ax = plt.subplots()
exp_path = '../makeup_exp/'

scheduling_policies = ["EDF", "RM", "FIFO"]


for workload_percent in range(20, 101, 20):
    datas = []
    for policy in scheduling_policies:
        datas.append(np.loadtxt(exp_path + policy+'-'+str(workload_percent) + '/latency', usecols=0, unpack=True))
        
    
    # Now, plot the box and wisker plot 
    bp = ax.boxplot(datas, labels=scheduling_policies, showfliers=False, showmeans=True)
    99th_percentile = [np.percentile(data, 99) for data in datas.T]
    ax.plot(scheduling_policies, 99th_percentile, 'rs')
    # It is necessary to adjust the tick range of the y axis
    # so that
    #   (1) the highest tick is higher than the largest data value, and
    #   (2) the smallest data value is visible
    ax.set_ylim(-3,50)

    ax.set_xlabel('Scheduling Policies')
    ax.set_ylabel('Latency (microseconds)')
    ax.set_title('The Box and Whisker Plot')

    #plt.show()
    plt.savefig('./latency-boxplot-'+str(workload_percent)+'.pdf')
