# Read the documentation:
# https://matplotlib.org/3.1.1/api/_as_gen/matplotlib.pyplot.boxplot.html

# Note that we do not need to always use the seaborn library
import numpy as np
import matplotlib.pyplot as plt

scheduling_policies = ["EDF", "RM", "FIFO"]


workload_percent = 100
fig1, ax = plt.subplots()
exp_path = '../makeup_exp/'

datas = []
for policy in scheduling_policies:
    data = np.loadtxt(exp_path + policy+'-'+str(workload_percent) + '/latency', usecols=0, unpack=True)
    datas.append(data)
    

# Now, plot the box and wisker plot 
ax.boxplot(datas, labels=scheduling_policies, showfliers=False)
positions = np.arange(len(scheduling_policies))+1

ninetynine_th_percentile = [np.percentile(data, 99) for data in datas]
ax.plot(positions, ninetynine_th_percentile, 'rs')
print(ninetynine_th_percentile)

mean_percentile = [np.mean(data) for data in datas]
ax.plot(positions, mean_percentile, 'b*')
print(mean_percentile)
# It is necessary to adjust the tick range of the y axis
# so that
#   (1) the highest tick is higher than the largest data value, and
#   (2) the smallest data value is visible
ax.set_ylim(-2,8000)

ax.set_xlabel('Scheduling Policies')
ax.set_ylabel('Latency (milliseconds)')
ax.set_title('Publisher-to-Subscriber Latency Under ' + str(workload_percent) +'% CPU Utilization')


#plt.show()
plt.savefig('./latency-boxplot-'+str(workload_percent)+'.pdf')
