# Read the documentation:
# https://matplotlib.org/3.1.1/api/_as_gen/matplotlib.pyplot.boxplot.html

# Note that we do not need to always use the seaborn library
import numpy as np
import matplotlib.pyplot as plt

scheduling_policies = ["EDF", "RM", "FIFO"]


for workload_percent in range(20, 101, 20):
    fig1, ax = plt.subplots()
    exp_path = '../makeup_exp/'

    datas = []
    for policy in scheduling_policies:
        data = np.loadtxt(exp_path + policy+'-'+str(workload_percent) + '/latency', usecols=0, unpack=True)
        datas.append(data)
        
    
    # Now, plot the box and wisker plot 
    bp = ax.boxplot(datas, labels=scheduling_policies, showfliers=False, showmeans=True)
    ninetynine_th_percentile = [np.percentile(data, 99) for data in datas]
    positions = np.arange(len(scheduling_policies))+1
    ax.plot(positions, ninetynine_th_percentile, 'rs')
    # It is necessary to adjust the tick range of the y axis
    # so that
    #   (1) the highest tick is higher than the largest data value, and
    #   (2) the smallest data value is visible
    ax.set_ylim(-2,20)

    ax.set_xlabel('Scheduling Policies')
    ax.set_ylabel('Latency (milliseconds)')
    ax.set_title('The Box and Whisker Plot')

    #plt.show()
    plt.savefig('./latency-boxplot-'+str(workload_percent)+'.pdf')
    plt.clf()
